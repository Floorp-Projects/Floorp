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
#include "mozilla/Endian.h"
#include "TexturePoolOGL.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/TextureHostOGL.h"

#include "gfxColor.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "gfxPrefs.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "GLReadTexImageHelper.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include <memory>
#include "mozilla/LinkedList.h"
#include "mozilla/Base64.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"
#include "nsISocketTransport.h"
#include "nsIServerSocket.h"
#include "nsReadLine.h"
#include "nsNetCID.h"
#include "nsIOutputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsIEventTarget.h"
#include "nsProxyRelease.h"

// Undo the damage done by mozzconf.h
#undef compress
#include "mozilla/Compression.h"

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
class DebugGLData;

/* This class handle websocket protocol which included
 * handshake and data frame's header
 */
class LayerScopeWebSocketHandler : public nsIInputStreamCallback {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    enum SocketStateType {
        NoHandshake,
        HandshakeSuccess,
        HandshakeFailed
    };

    LayerScopeWebSocketHandler()
        : mState(NoHandshake)
    { }

private:
    virtual ~LayerScopeWebSocketHandler()
    {
        if (mTransport) {
            mTransport->Close(NS_OK);
        }
    }

public:
    void OpenStream(nsISocketTransport* aTransport) {
        MOZ_ASSERT(aTransport);

        mTransport = aTransport;
        mTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                     0,
                                     0,
                                     getter_AddRefs(mOutputStream));

        nsCOMPtr<nsIInputStream> debugInputStream;
        mTransport->OpenInputStream(0,
                                    0,
                                    0,
                                    getter_AddRefs(debugInputStream));
        mInputStream = do_QueryInterface(debugInputStream);
        mInputStream->AsyncWait(this, 0, 0, NS_GetCurrentThread());
    }

    bool WriteToStream(void *ptr, uint32_t size) {
        if (mState == NoHandshake) {
            // Not yet handshake, just return true in case of
            // LayerScope remove this handle
            return true;
        } else if (mState == HandshakeFailed) {
            return false;
        }

        // Generate WebSocket header
        uint8_t wsHeader[10];
        int wsHeaderSize = 0;
        const uint8_t opcode = 0x2;
        wsHeader[0] = 0x80 | (opcode & 0x0f); // FIN + opcode;
        if (size <= 125) {
            wsHeaderSize = 2;
            wsHeader[1] = size;
        } else if (size < 65536) {
            wsHeaderSize = 4;
            wsHeader[1] = 0x7E;
            NetworkEndian::writeUint16(wsHeader + 2, size);
        } else {
            wsHeaderSize = 10;
            wsHeader[1] = 0x7F;
            NetworkEndian::writeUint64(wsHeader + 2, size);
        }

        // Send WebSocket header
        nsresult rv;
        uint32_t cnt;
        rv = mOutputStream->Write(reinterpret_cast<char*>(wsHeader),
                                 wsHeaderSize, &cnt);
        if (NS_FAILED(rv))
            return false;

        uint32_t written = 0;
        while (written < size) {
            uint32_t cnt;
            rv = mOutputStream->Write(reinterpret_cast<char*>(ptr) + written,
                                     size - written, &cnt);
            if (NS_FAILED(rv))
                return false;

            written += cnt;
        }

        return true;
    }

    // nsIInputStreamCallback
    NS_IMETHODIMP OnInputStreamReady(nsIAsyncInputStream *stream) MOZ_OVERRIDE
    {
        nsTArray<nsCString> protocolString;
        ReadInputStreamData(protocolString);

        if (WebSocketHandshake(protocolString)) {
            mState = HandshakeSuccess;
        } else {
            mState = HandshakeFailed;
        }
        return NS_OK;
    }
private:
    void ReadInputStreamData(nsTArray<nsCString>& aProtocolString)
    {
        nsLineBuffer<char> lineBuffer;
        nsCString line;
        bool more = true;
        do {
            NS_ReadLine(mInputStream.get(), &lineBuffer, line, &more);

            if (line.Length() > 0) {
                aProtocolString.AppendElement(line);
            }
        } while (more && line.Length() > 0);
    }

    bool WebSocketHandshake(nsTArray<nsCString>& aProtocolString)
    {
        nsresult rv;
        bool isWebSocket = false;
        nsCString version;
        nsCString wsKey;
        nsCString protocol;

        // Validate WebSocket client request.
        if (aProtocolString.Length() == 0)
            return false;

        // Check that the HTTP method is GET
        const char* HTTP_METHOD = "GET ";
        if (strncmp(aProtocolString[0].get(), HTTP_METHOD, strlen(HTTP_METHOD)) != 0) {
            return false;
        }

        for (uint32_t i = 1; i < aProtocolString.Length(); ++i) {
            const char* line = aProtocolString[i].get();
            const char* prop_pos = strchr(line, ':');
            if (prop_pos != nullptr) {
                nsCString key(line, prop_pos - line);
                nsCString value(prop_pos + 2);
                if (key.EqualsIgnoreCase("upgrade") &&
                    value.EqualsIgnoreCase("websocket")) {
                    isWebSocket = true;
                } else if (key.EqualsIgnoreCase("sec-websocket-version")) {
                    version = value;
                } else if (key.EqualsIgnoreCase("sec-websocket-key")) {
                    wsKey = value;
                } else if (key.EqualsIgnoreCase("sec-websocket-protocol")) {
                    protocol = value;
                }
            }
        }

        if (!isWebSocket) {
            return false;
        }

        if (!(version.EqualsLiteral("7") ||
              version.EqualsLiteral("8") ||
              version.EqualsLiteral("13"))) {
            return false;
        }

        if (!(protocol.EqualsIgnoreCase("binary"))) {
            return false;
        }

        // Client request is valid. Start to generate and send server response.
        nsAutoCString guid("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        nsAutoCString res;
        SHA1Sum sha1;
        nsCString combined(wsKey + guid);
        sha1.update(combined.get(), combined.Length());
        uint8_t digest[SHA1Sum::HashSize]; // SHA1 digests are 20 bytes long.
        sha1.finish(digest);
        nsCString newString(reinterpret_cast<char*>(digest), SHA1Sum::HashSize);
        Base64Encode(newString, res);

        nsCString response("HTTP/1.1 101 Switching Protocols\r\n");
        response.AppendLiteral("Upgrade: websocket\r\n");
        response.AppendLiteral("Connection: Upgrade\r\n");
        response.Append(nsCString("Sec-WebSocket-Accept: ") + res + nsCString("\r\n"));
        response.AppendLiteral("Sec-WebSocket-Protocol: binary\r\n\r\n");
        uint32_t written = 0;
        uint32_t size = response.Length();
        while (written < size) {
            uint32_t cnt;
            rv = mOutputStream->Write(const_cast<char*>(response.get()) + written,
                                     size - written, &cnt);
            if (NS_FAILED(rv))
                return false;

            written += cnt;
        }
        mOutputStream->Flush();

        return true;
    }

    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsIAsyncInputStream> mInputStream;
    nsCOMPtr<nsISocketTransport> mTransport;
    SocketStateType mState;
};

NS_IMPL_ISUPPORTS(LayerScopeWebSocketHandler, nsIInputStreamCallback);

class LayerScopeWebSocketManager {
public:
    LayerScopeWebSocketManager();
    ~LayerScopeWebSocketManager();

    void AddConnection(nsISocketTransport *aTransport)
    {
        MOZ_ASSERT(aTransport);
        nsRefPtr<LayerScopeWebSocketHandler> temp = new LayerScopeWebSocketHandler();
        temp->OpenStream(aTransport);
        mHandlers.AppendElement(temp.get());
    }

    void RemoveConnection(uint32_t aIndex)
    {
        MOZ_ASSERT(aIndex < mHandlers.Length());
        mHandlers.RemoveElementAt(aIndex);
    }

    void RemoveAllConnections()
    {
        mHandlers.Clear();
    }

    bool WriteAll(void *ptr, uint32_t size)
    {
        for (int32_t i = mHandlers.Length() - 1; i >= 0; --i) {
            if (!mHandlers[i]->WriteToStream(ptr, size)) {
                // Send failed, remove this handler
                RemoveConnection(i);
            }
        }

        return true;
    }

    bool IsConnected()
    {
        return (mHandlers.Length() != 0) ? true : false;
    }

    void AppendDebugData(DebugGLData *aDebugData);
    void CleanDebugData();
    void DispatchDebugData();
private:
    nsTArray<nsRefPtr<LayerScopeWebSocketHandler> > mHandlers;
    nsCOMPtr<nsIThread> mDebugSenderThread;
    nsRefPtr<DebugDataSender> mCurrentSender;
    nsCOMPtr<nsIServerSocket> mServerSocket;
};

// Static class to create and destory LayerScopeWebSocketManager object
class WebSocketHelper
{
public:
    static void CreateServerSocket()
    {
        // Create Web Server Socket (which has to be on the main thread)
        NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
        if (!sWebSocketManager) {
            sWebSocketManager = new LayerScopeWebSocketManager();
        }
    }

    static void DestroyServerSocket()
    {
        // Destroy Web Server Socket
        if (sWebSocketManager) {
            sWebSocketManager->RemoveAllConnections();
        }
    }

    static LayerScopeWebSocketManager* GetSocketManager()
    {
        return sWebSocketManager.get();
    }

private:
    static StaticAutoPtr<LayerScopeWebSocketManager> sWebSocketManager;
};

StaticAutoPtr<LayerScopeWebSocketManager> WebSocketHelper::sWebSocketManager;

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
        if (!WebSocketHelper::GetSocketManager())
            return true;
        return WebSocketHelper::GetSocketManager()->WriteAll(ptr, size);
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
    DebugGLTextureData(GLContext* cx,
                       void* layerRef,
                       GLenum target,
                       GLuint name,
                       DataSourceSurface* img)
        : DebugGLData(DebugGLData::TextureData, cx),
          mLayerRef(layerRef),
          mTarget(target),
          mName(name),
          mDatasize(0)
    {
        // pre-packing
        // DataSourceSurface may have locked buffer,
        // so we should compress now, and then it could
        // be unlocked outside.
        pack(img);
    }

    void *GetLayerRef() const { return mLayerRef; }
    GLuint GetName() const { return mName; }
    GLenum GetTextureTarget() const { return mTarget; }

    virtual bool Write() {
        // write the packet header data
        if (!WriteToStream(&mPacket, sizeof(mPacket)))
            return false;

        // then the image data
        if (mCompresseddata.get() && !WriteToStream(mCompresseddata, mDatasize))
            return false;

        // then pad out to 4 bytes
        if (mDatasize % 4 != 0) {
            static char buf[] = { 0, 0, 0, 0 };
            if (!WriteToStream(buf, 4 - (mDatasize % 4)))
                return false;
        }

        return true;
    }

private:
    void pack(DataSourceSurface* aImage) {
        mPacket.type = mDataType;
        mPacket.ptr = static_cast<uint64_t>(mContextAddress);
        mPacket.layerref = reinterpret_cast<uint64_t>(mLayerRef);
        mPacket.name = mName;
        mPacket.format = 0;
        mPacket.target = mTarget;
        mPacket.dataFormat = LOCAL_GL_RGBA;

        if (aImage) {
            mPacket.width = aImage->GetSize().width;
            mPacket.height = aImage->GetSize().height;
            mPacket.stride = aImage->Stride();
            mPacket.dataSize = aImage->GetSize().height * aImage->Stride();

            mCompresseddata =
                new char[LZ4::maxCompressedSize(mPacket.dataSize)];
            if (mCompresseddata.get()) {
                int ndatasize = LZ4::compress((char*)aImage->GetData(),
                                              mPacket.dataSize,
                                              mCompresseddata);
                if (ndatasize > 0) {
                    mDatasize = ndatasize;

                    mPacket.dataFormat = (1 << 16) | mPacket.dataFormat;
                    mPacket.dataSize = mDatasize;
                } else {
                    NS_WARNING("Compress data failed");
                }
            } else {
                NS_WARNING("Couldn't moz_malloc for compressed data.");
            }
        } else {
            mPacket.width = 0;
            mPacket.height = 0;
            mPacket.stride = 0;
            mPacket.dataSize = 0;
        }
    }

protected:
    void* mLayerRef;
    GLenum mTarget;
    GLuint mName;

    // Packet data
    DebugGLData::TexturePacket mPacket;
    nsAutoArrayPtr<char> mCompresseddata;
    uint32_t mDatasize;
};

class DebugGLColorData : public DebugGLData {
public:
    DebugGLColorData(void* layerRef, const gfxRGBA& color, int width, int height)
        : DebugGLData(DebugGLData::ColorData),
          mLayerRef(layerRef),
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

class DebugListener : public nsIServerSocketListener
{
    virtual ~DebugListener() { }

public:

    NS_DECL_THREADSAFE_ISUPPORTS

    DebugListener() { }

    /* nsIServerSocketListener */

    NS_IMETHODIMP OnSocketAccepted(nsIServerSocket *aServ,
                                   nsISocketTransport *aTransport)
    {
        if (!WebSocketHelper::GetSocketManager())
            return NS_OK;

        printf_stderr("*** LayerScope: Accepted connection\n");
        WebSocketHelper::GetSocketManager()->AddConnection(aTransport);
        return NS_OK;
    }

    NS_IMETHODIMP OnStopListening(nsIServerSocket *aServ,
                                  nsresult aStatus)
    {
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(DebugListener, nsIServerSocketListener);


class DebugDataSender : public nsIRunnable
{
    virtual ~DebugDataSender() {
        Cleanup();
    }

public:

    NS_DECL_THREADSAFE_ISUPPORTS

    DebugDataSender() { }

    void Append(DebugGLData *d) {
        mList.insertBack(d);
    }

    void Cleanup() {
        if (mList.isEmpty())
            return;

        DebugGLData *d;
        while ((d = mList.popFirst()) != nullptr)
            delete d;
    }

    /* nsIRunnable impl; send the data */

    NS_IMETHODIMP Run() {
        DebugGLData *d;
        nsresult rv = NS_OK;

        while ((d = mList.popFirst()) != nullptr) {
            nsAutoPtr<DebugGLData> cleaner(d);
            if (!d->Write()) {
                rv = NS_ERROR_FAILURE;
                break;
            }
        }

        Cleanup();

        if (NS_FAILED(rv)) {
            WebSocketHelper::DestroyServerSocket();
        }

        return NS_OK;
    }

protected:
    LinkedList<DebugGLData> mList;
};

NS_IMPL_ISUPPORTS(DebugDataSender, nsIRunnable);

/*
 * LayerScope SendXXX Structure
 * 1. SendLayer
 * 2. SendEffectChain
 *   1. SendTexturedEffect
 *      -> SendTextureSource
 *   2. SendYCbCrEffect
 *      -> SendTextureSource
 *   3. SendColor
 */
class SenderHelper
{
// Sender public APIs
public:
    static void SendLayer(LayerComposite* aLayer,
                          int aWidth,
                          int aHeight);

    static void SendEffectChain(gl::GLContext* aGLContext,
                                const EffectChain& aEffectChain,
                                int aWidth = 0,
                                int aHeight = 0);

// Sender private functions
private:
    static void SendColor(void* aLayerRef,
                          const gfxRGBA& aColor,
                          int aWidth,
                          int aHeight);
    static void SendTextureSource(GLContext* aGLContext,
                                  void* aLayerRef,
                                  TextureSourceOGL* aSource,
                                  bool aFlipY);
    static void SendTexturedEffect(GLContext* aGLContext,
                                   void* aLayerRef,
                                   const TexturedEffect* aEffect);
    static void SendYCbCrEffect(GLContext* aGLContext,
                                void* aLayerRef,
                                const EffectYCbCr* aEffect);
};


// ----------------------------------------------
// SenderHelper implementation
// ----------------------------------------------
void
SenderHelper::SendLayer(LayerComposite* aLayer,
                        int aWidth,
                        int aHeight)
{
    MOZ_ASSERT(aLayer && aLayer->GetLayer());
    if (!aLayer || !aLayer->GetLayer()) {
        return;
    }

    switch (aLayer->GetLayer()->GetType()) {
        case Layer::TYPE_COLOR: {
            EffectChain effect;
            aLayer->GenEffectChain(effect);
            SenderHelper::SendEffectChain(nullptr, effect, aWidth, aHeight);
            break;
        }
        case Layer::TYPE_IMAGE:
        case Layer::TYPE_CANVAS:
        case Layer::TYPE_THEBES: {
            // Get CompositableHost and Compositor
            CompositableHost* compHost = aLayer->GetCompositableHost();
            Compositor* comp = compHost->GetCompositor();
            // Send EffectChain only for CompositorOGL
            if (LayersBackend::LAYERS_OPENGL == comp->GetBackendType()) {
                CompositorOGL* compOGL = static_cast<CompositorOGL*>(comp);
                EffectChain effect;
                // Generate primary effect (lock and gen)
                AutoLockCompositableHost lock(compHost);
                aLayer->GenEffectChain(effect);
                SenderHelper::SendEffectChain(compOGL->gl(), effect);
            }
            break;
        }
        case Layer::TYPE_CONTAINER:
        default:
            break;
    }
}

void
SenderHelper::SendColor(void* aLayerRef,
                        const gfxRGBA& aColor,
                        int aWidth,
                        int aHeight)
{
    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLColorData(aLayerRef, aColor, aWidth, aHeight));
}

void
SenderHelper::SendTextureSource(GLContext* aGLContext,
                                void* aLayerRef,
                                TextureSourceOGL* aSource,
                                bool aFlipY)
{
    MOZ_ASSERT(aGLContext);
    if (!aGLContext) {
        return;
    }

    GLenum textureTarget = aSource->GetTextureTarget();
    ShaderConfigOGL config = ShaderConfigFromTargetAndFormat(textureTarget,
                                                             aSource->GetFormat());
    int shaderConfig = config.mFeatures;

    aSource->BindTexture(LOCAL_GL_TEXTURE0, gfx::Filter::LINEAR);

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
    RefPtr<DataSourceSurface> img =
        aGLContext->ReadTexImageHelper()->ReadTexImage(0, textureTarget,
                                                       size,
                                                       shaderConfig, aFlipY);

    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLTextureData(aGLContext, aLayerRef, textureTarget,
                               textureId, img));
}

void
SenderHelper::SendTexturedEffect(GLContext* aGLContext,
                                 void* aLayerRef,
                                 const TexturedEffect* aEffect)
{
    TextureSourceOGL* source = aEffect->mTexture->AsSourceOGL();
    if (!source)
        return;

    bool flipY = false;
    SendTextureSource(aGLContext, aLayerRef, source, flipY);
}

void
SenderHelper::SendYCbCrEffect(GLContext* aGLContext,
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
SenderHelper::SendEffectChain(GLContext* aGLContext,
                              const EffectChain& aEffectChain,
                              int aWidth,
                              int aHeight)
{
    const Effect* primaryEffect = aEffectChain.mPrimaryEffect;
    switch (primaryEffect->mType) {
        case EffectTypes::RGB: {
            const TexturedEffect* texturedEffect =
                static_cast<const TexturedEffect*>(primaryEffect);
            SendTexturedEffect(aGLContext, aEffectChain.mLayerRef, texturedEffect);
            break;
        }
        case EffectTypes::YCBCR: {
            const EffectYCbCr* yCbCrEffect =
                static_cast<const EffectYCbCr*>(primaryEffect);
            SendYCbCrEffect(aGLContext, aEffectChain.mLayerRef, yCbCrEffect);
            break;
        }
        case EffectTypes::SOLID_COLOR: {
            const EffectSolidColor* solidColorEffect =
                static_cast<const EffectSolidColor*>(primaryEffect);
            gfxRGBA color(solidColorEffect->mColor.r,
                          solidColorEffect->mColor.g,
                          solidColorEffect->mColor.b,
                          solidColorEffect->mColor.a);
            SendColor(aEffectChain.mLayerRef, color, aWidth, aHeight);
            break;
        }
        case EffectTypes::COMPONENT_ALPHA:
        case EffectTypes::RENDER_TARGET:
        default:
            break;
    }

    //const Effect* secondaryEffect = aEffectChain.mSecondaryEffects[EffectTypes::MASK];
    // TODO:
}

// ----------------------------------------------
// LayerScopeWebSocketManager implementation
// ----------------------------------------------
LayerScopeWebSocketManager::LayerScopeWebSocketManager()
{
    NS_NewThread(getter_AddRefs(mDebugSenderThread));

    mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
    int port = gfxPrefs::LayerScopePort();
    mServerSocket->Init(port, false, -1);
    mServerSocket->AsyncListen(new DebugListener);
}

LayerScopeWebSocketManager::~LayerScopeWebSocketManager()
{
}

void
LayerScopeWebSocketManager::AppendDebugData(DebugGLData *aDebugData)
{
    if (!mCurrentSender) {
        mCurrentSender = new DebugDataSender();
    }

    mCurrentSender->Append(aDebugData);
}

void
LayerScopeWebSocketManager::CleanDebugData()
{
    if (mCurrentSender) {
        mCurrentSender->Cleanup();
    }
}

void
LayerScopeWebSocketManager::DispatchDebugData()
{
    mDebugSenderThread->Dispatch(mCurrentSender, NS_DISPATCH_NORMAL);
    mCurrentSender = nullptr;
}


// ----------------------------------------------
// LayerScope implementation
// ----------------------------------------------
void
LayerScope::Init()
{
    if (!gfxPrefs::LayerScopeEnabled()) {
        return;
    }

    // Note: The server socket has to be created on the main thread
    WebSocketHelper::CreateServerSocket();
}

void
LayerScope::DeInit()
{
    // Destroy Web Server Socket
    WebSocketHelper::DestroyServerSocket();
}

void
LayerScope::SendEffectChain(gl::GLContext* aGLContext,
                            const EffectChain& aEffectChain,
                            int aWidth,
                            int aHeight)
{
    // Protect this public function
    if (!CheckSendable()) {
        return;
    }
    SenderHelper::SendEffectChain(aGLContext, aEffectChain, aWidth, aHeight);
}

void
LayerScope::SendLayer(LayerComposite* aLayer,
                      int aWidth,
                      int aHeight)
{
    // Protect this public function
    if (!CheckSendable()) {
        return;
    }
    SenderHelper::SendLayer(aLayer, aWidth, aHeight);
}

bool
LayerScope::CheckSendable()
{
    if (!WebSocketHelper::GetSocketManager()) {
        return false;
    }
    if (!WebSocketHelper::GetSocketManager()->IsConnected()) {
        return false;
    }
    return true;
}

void
LayerScope::CleanLayer()
{
    if (CheckSendable()) {
        WebSocketHelper::GetSocketManager()->CleanDebugData();
    }
}

// ----------------------------------------------
// LayerScopeAutoFrame implementation
// ----------------------------------------------
LayerScopeAutoFrame::LayerScopeAutoFrame(int64_t aFrameStamp)
{
    // Do Begin Frame
    BeginFrame(aFrameStamp);
}

LayerScopeAutoFrame::~LayerScopeAutoFrame()
{
    // Do End Frame
    EndFrame();
}

void
LayerScopeAutoFrame::BeginFrame(int64_t aFrameStamp)
{
    if (!LayerScope::CheckSendable()) {
        return;
    }

    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLData(DebugGLData::FrameStart, nullptr, aFrameStamp));
}

void
LayerScopeAutoFrame::EndFrame()
{
    if (!LayerScope::CheckSendable()) {
        return;
    }

    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLData(DebugGLData::FrameEnd, nullptr));
    WebSocketHelper::GetSocketManager()->DispatchDebugData();
}

} /* layers */
} /* mozilla */
