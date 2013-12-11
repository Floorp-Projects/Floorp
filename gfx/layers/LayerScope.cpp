/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "LayerScope.h"

#include "Composer2D.h"
#include "Effects.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "TexturePoolOGL.h"
#include "mozilla/layers/TextureHostOGL.h"

#include "gfxColor.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include <memory>
#include "mozilla/Compression.h"
#include "mozilla/LinkedList.h"
#include "nsThreadUtils.h"
#include "nsISocketTransport.h"
#include "nsIServerSocket.h"
#include "nsNetCID.h"
#include "nsIOutputStream.h"
#include "nsIEventTarget.h"
#include "nsProxyRelease.h"

#ifdef __GNUC__
#define PACKED_STRUCT __attribute__((packed))
#else
#define PACKED_STRUCT
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::Compression;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla;

class DebugDataSender;

static bool gDebugConnected = false;
static nsCOMPtr<nsIServerSocket> gDebugServerSocket;
static nsCOMPtr<nsIThread> gDebugSenderThread;
static nsCOMPtr<nsISocketTransport> gDebugSenderTransport;
static nsCOMPtr<nsIOutputStream> gDebugStream;
static nsCOMPtr<DebugDataSender> gCurrentSender;

class DebugGLData : public LinkedListElement<DebugGLData> {
public:
    typedef enum {
        FrameStart,
        FrameEnd,
        TextureData,
        ColorData
    } DataType;

    virtual ~DebugGLData() { }

    DataType GetDataType() const { return mDataType; }
    intptr_t GetContextAddress() const { return mContextAddress; }
    int64_t GetValue() const { return mValue; }

    DebugGLData(DataType dataType)
        : mDataType(dataType),
          mContextAddress(0),
          mValue(0)
    { }

    DebugGLData(DataType dataType, GLContext* cx)
        : mDataType(dataType),
          mContextAddress(reinterpret_cast<intptr_t>(cx)),
          mValue(0)
    { }

    DebugGLData(DataType dataType, GLContext* cx, int64_t value)
        : mDataType(dataType),
          mContextAddress(reinterpret_cast<intptr_t>(cx)),
          mValue(value)
    { }

    virtual bool Write() {
        if (mDataType != FrameStart &&
            mDataType != FrameEnd)
        {
            NS_WARNING("Unimplemented data type!");
            return false;
        }

        DebugGLData::BasicPacket packet;

        packet.type = mDataType;
        packet.ptr = static_cast<uint64_t>(mContextAddress);
        packet.value = mValue;

        return WriteToStream(&packet, sizeof(packet));
    }

    static bool WriteToStream(void *ptr, uint32_t size) {
        uint32_t written = 0;
        nsresult rv;
        while (written < size) {
            uint32_t cnt;
            rv = gDebugStream->Write(reinterpret_cast<char*>(ptr) + written,
                                     size - written, &cnt);
            if (NS_FAILED(rv))
                return false;

            written += cnt;
        }

        return true;
    }

protected:
    DataType mDataType;
    intptr_t mContextAddress;
    int64_t mValue;

public:
  // the data packet formats; all packed
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
    typedef struct {
        uint32_t type;
        uint64_t ptr;
        uint64_t value;
    } PACKED_STRUCT BasicPacket;

    typedef struct {
        uint32_t type;
        uint64_t ptr;
        uint64_t layerref;
        uint32_t color;
        uint32_t width;
        uint32_t height;
    } PACKED_STRUCT ColorPacket;

    typedef struct {
        uint32_t type;
        uint64_t ptr;
        uint64_t layerref;
        uint32_t name;
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        uint32_t format;
        uint32_t target;
        uint32_t dataFormat;
        uint32_t dataSize;
    } PACKED_STRUCT TexturePacket;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
};

class DebugGLTextureData : public DebugGLData {
public:
    DebugGLTextureData(GLContext* cx, void* layerRef, GLuint target, GLenum name, gfxImageSurface* img)
        : DebugGLData(DebugGLData::TextureData, cx),
          mLayerRef(layerRef),
          mTarget(target),
          mName(name),
          mImage(img)
    { }

    void *GetLayerRef() const { return mLayerRef; }
    GLuint GetName() const { return mName; }
    gfxImageSurface* GetImage() const { return mImage; }
    GLenum GetTextureTarget() const { return mTarget; }

    virtual bool Write() {
        DebugGLData::TexturePacket packet;
        char* dataptr = nullptr;
        uint32_t datasize = 0;
        std::auto_ptr<char> compresseddata;

        packet.type = mDataType;
        packet.ptr = static_cast<uint64_t>(mContextAddress);
        packet.layerref = reinterpret_cast<uint64_t>(mLayerRef);
        packet.name = mName;
        packet.format = 0;
        packet.target = mTarget;
        packet.dataFormat = LOCAL_GL_RGBA;

        if (mImage) {
            packet.width = mImage->Width();
            packet.height = mImage->Height();
            packet.stride = mImage->Stride();
            packet.dataSize = mImage->GetDataSize();

            dataptr = (char*) mImage->Data();
            datasize = mImage->GetDataSize();

            compresseddata = std::auto_ptr<char>((char*) moz_malloc(LZ4::maxCompressedSize(datasize)));
            if (compresseddata.get()) {
                int ndatasize = LZ4::compress(dataptr, datasize, compresseddata.get());
                if (ndatasize > 0) {
                    datasize = ndatasize;
                    dataptr = compresseddata.get();

                    packet.dataFormat = (1 << 16) | packet.dataFormat;
                    packet.dataSize = datasize;
                }
            }
        } else {
            packet.width = 0;
            packet.height = 0;
            packet.stride = 0;
            packet.dataSize = 0;
        }

        // write the packet header data
        if (!WriteToStream(&packet, sizeof(packet)))
            return false;

        // then the image data
        if (!WriteToStream(dataptr, datasize))
            return false;

        // then pad out to 4 bytes
        if (datasize % 4 != 0) {
            static char buf[] = { 0, 0, 0, 0 };
            if (!WriteToStream(buf, 4 - (datasize % 4)))
                return false;
        }

        return true;
    }

protected:
    void* mLayerRef;
    GLenum mTarget;
    GLuint mName;
    nsRefPtr<gfxImageSurface> mImage;
};

class DebugGLColorData : public DebugGLData {
public:
    DebugGLColorData(void* layerRef, const gfxRGBA& color, int width, int height)
        : DebugGLData(DebugGLData::ColorData),
          mColor(color.Packed()),
          mSize(width, height)
    { }

    void *GetLayerRef() const { return mLayerRef; }
    uint32_t GetColor() const { return mColor; }
    const nsIntSize& GetSize() const { return mSize; }

    virtual bool Write() {
        DebugGLData::ColorPacket packet;

        packet.type = mDataType;
        packet.ptr = static_cast<uint64_t>(mContextAddress);
        packet.layerref = reinterpret_cast<uintptr_t>(mLayerRef);
        packet.color = mColor;
        packet.width = mSize.width;
        packet.height = mSize.height;

        return WriteToStream(&packet, sizeof(packet));
    }

protected:
    void *mLayerRef;
    uint32_t mColor;
    nsIntSize mSize;
};

static bool
CheckSender()
{
    if (!gDebugConnected)
        return false;

    // At some point we may want to support sending
    // data in between frames.
#if 1
    if (!gCurrentSender)
        return false;
#else
    if (!gCurrentSender)
        gCurrentSender = new DebugDataSender();
#endif

    return true;
}


class DebugListener : public nsIServerSocketListener
{
public:

    NS_DECL_THREADSAFE_ISUPPORTS

    DebugListener() { }
    virtual ~DebugListener() { }

    /* nsIServerSocketListener */

    NS_IMETHODIMP OnSocketAccepted(nsIServerSocket *aServ,
                                   nsISocketTransport *aTransport)
    {
        printf_stderr("*** LayerScope: Accepted connection\n");
        gDebugConnected = true;
        gDebugSenderTransport = aTransport;
        aTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING, 0, 0, getter_AddRefs(gDebugStream));
        return NS_OK;
    }

    NS_IMETHODIMP OnStopListening(nsIServerSocket *aServ,
                                  nsresult aStatus)
    {
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(DebugListener, nsIServerSocketListener);


class DebugDataSender : public nsIRunnable
{
public:

    NS_DECL_THREADSAFE_ISUPPORTS

    DebugDataSender() {
        mList = new LinkedList<DebugGLData>();
    }

    virtual ~DebugDataSender() {
        Cleanup();
    }

    void Append(DebugGLData *d) {
        mList->insertBack(d);
    }

    void Cleanup() {
        if (!mList)
            return;

        DebugGLData *d;
        while ((d = mList->popFirst()) != nullptr)
            delete d;
        delete mList;

        mList = nullptr;
    }

    /* nsIRunnable impl; send the data */

    NS_IMETHODIMP Run() {
        DebugGLData *d;
        nsresult rv = NS_OK;

        // If we got closed while trying to write some stuff earlier, just
        // throw away everything.
        if (!gDebugStream) {
            Cleanup();
            return NS_OK;
        }

        while ((d = mList->popFirst()) != nullptr) {
            std::auto_ptr<DebugGLData> cleaner(d);
            if (!d->Write()) {
                rv = NS_ERROR_FAILURE;
                break;
            }
        }

        Cleanup();

        if (NS_FAILED(rv)) {
            gDebugSenderTransport->Close(rv);
            gDebugConnected = false;
            gDebugStream = nullptr;
            gDebugServerSocket = nullptr;
        }

        return NS_OK;
    }

protected:
    LinkedList<DebugGLData> *mList;
};

NS_IMPL_ISUPPORTS1(DebugDataSender, nsIRunnable);

void
LayerScope::CreateServerSocket()
{
    if (!Preferences::GetBool("gfx.layerscope.enabled", false)) {
        return;
    }

    if (!gDebugSenderThread) {
        NS_NewThread(getter_AddRefs(gDebugSenderThread));
    }

    if (!gDebugServerSocket) {
        gDebugServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
        int port = Preferences::GetInt("gfx.layerscope.port", 23456);
        gDebugServerSocket->Init(port, false, -1);
        gDebugServerSocket->AsyncListen(new DebugListener);
    }
}

void
LayerScope::DestroyServerSocket()
{
    gDebugConnected = false;
    gDebugStream = nullptr;
    gDebugServerSocket = nullptr;
}

void
LayerScope::BeginFrame(GLContext* aGLContext, int64_t aFrameStamp)
{
    if (!gDebugConnected)
        return;

#if 0
    // if we're sending data in between frames, flush the list down the socket,
    // and start a new one
    if (gCurrentSender) {
        gDebugSenderThread->Dispatch(gCurrentSender, NS_DISPATCH_NORMAL);
    }
#endif

    gCurrentSender = new DebugDataSender();
    gCurrentSender->Append(new DebugGLData(DebugGLData::FrameStart, aGLContext, aFrameStamp));
}

void
LayerScope::EndFrame(GLContext* aGLContext)
{
    if (!CheckSender())
        return;

    gCurrentSender->Append(new DebugGLData(DebugGLData::FrameEnd, aGLContext));
    gDebugSenderThread->Dispatch(gCurrentSender, NS_DISPATCH_NORMAL);
    gCurrentSender = nullptr;
}

static void
SendColor(void* aLayerRef, const gfxRGBA& aColor, int aWidth, int aHeight)
{
    if (!CheckSender())
        return;

    gCurrentSender->Append(
        new DebugGLColorData(aLayerRef, aColor, aWidth, aHeight));
}

static void
SendTextureSource(GLContext* aGLContext,
                  void* aLayerRef,
                  TextureSourceOGL* aSource,
                  bool aFlipY)
{
    GLenum textureTarget = aSource->GetTextureTarget();
    int shaderProgram =
        (int) ShaderProgramFromTargetAndFormat(textureTarget,
                                               aSource->GetFormat());

    aSource->BindTexture(LOCAL_GL_TEXTURE0);

    GLuint textureId = 0;
    // This is horrid hack. It assumes that aGLContext matches the context
    // aSource has bound to.
    if (textureTarget == LOCAL_GL_TEXTURE_2D) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &textureId);
    } else if (textureTarget == LOCAL_GL_TEXTURE_EXTERNAL) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_EXTERNAL, &textureId);
    } else if (textureTarget == LOCAL_GL_TEXTURE_RECTANGLE) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_RECTANGLE, &textureId);
    }

    gfx::IntSize size = aSource->GetSize();

    // By sending 0 to ReadTextureImage rely upon aSource->BindTexture binding
    // texture correctly. textureId is used for tracking in DebugGLTextureData.
    nsRefPtr<gfxImageSurface> img =
        aGLContext->ReadTextureImage(0, textureTarget,
                                     gfxIntSize(size.width, size.height),
                                     shaderProgram, aFlipY);

    gCurrentSender->Append(
        new DebugGLTextureData(aGLContext, aLayerRef, textureTarget,
                               textureId, img));
}

static void
SendTexturedEffect(GLContext* aGLContext,
                   void* aLayerRef,
                   const TexturedEffect* aEffect)
{
    TextureSourceOGL* source = aEffect->mTexture->AsSourceOGL();
    if (!source)
        return;

    bool flipY = false;
    SendTextureSource(aGLContext, aLayerRef, source, flipY);
}

static void
SendYCbCrEffect(GLContext* aGLContext,
                void* aLayerRef,
                const EffectYCbCr* aEffect)
{
    TextureSource* sourceYCbCr = aEffect->mTexture;
    if (!sourceYCbCr)
        return;

    const int Y = 0, Cb = 1, Cr = 2;
    TextureSourceOGL* sourceY =  sourceYCbCr->GetSubSource(Y)->AsSourceOGL();
    TextureSourceOGL* sourceCb = sourceYCbCr->GetSubSource(Cb)->AsSourceOGL();
    TextureSourceOGL* sourceCr = sourceYCbCr->GetSubSource(Cr)->AsSourceOGL();

    bool flipY = false;
    SendTextureSource(aGLContext, aLayerRef, sourceY,  flipY);
    SendTextureSource(aGLContext, aLayerRef, sourceCb, flipY);
    SendTextureSource(aGLContext, aLayerRef, sourceCr, flipY);
}

void
LayerScope::SendEffectChain(GLContext* aGLContext,
                            const EffectChain& aEffectChain,
                            int aWidth, int aHeight)
{
    if (!CheckSender())
        return;

    const Effect* primaryEffect = aEffectChain.mPrimaryEffect;
    switch (primaryEffect->mType) {
    case EFFECT_BGRX:
    case EFFECT_RGBX:
    case EFFECT_BGRA:
    case EFFECT_RGBA:
    {
        const TexturedEffect* texturedEffect =
            static_cast<const TexturedEffect*>(primaryEffect);
        SendTexturedEffect(aGLContext, aEffectChain.mLayerRef, texturedEffect);
    }
    break;
    case EFFECT_YCBCR:
    {
        const EffectYCbCr* yCbCrEffect =
            static_cast<const EffectYCbCr*>(primaryEffect);
        SendYCbCrEffect(aGLContext, aEffectChain.mLayerRef, yCbCrEffect);
    }
    case EFFECT_SOLID_COLOR:
    {
        const EffectSolidColor* solidColorEffect =
            static_cast<const EffectSolidColor*>(primaryEffect);
        gfxRGBA color(solidColorEffect->mColor.r,
                      solidColorEffect->mColor.g,
                      solidColorEffect->mColor.b,
                      solidColorEffect->mColor.a);
        SendColor(aEffectChain.mLayerRef, color, aWidth, aHeight);
    }
    break;
    case EFFECT_COMPONENT_ALPHA:
    case EFFECT_RENDER_TARGET:
    default:
        break;
    }

    //const Effect* secondaryEffect = aEffectChain.mSecondaryEffects[EFFECT_MASK];
    // TODO:
}

} /* layers */
} /* mozilla */
