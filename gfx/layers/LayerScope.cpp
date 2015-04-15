/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "LayerScope.h"

#include "nsAppRunner.h"
#include "Composer2D.h"
#include "Effects.h"
#include "mozilla/Endian.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"

#include "TexturePoolOGL.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorParent.h"
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
#include <list>

// Undo the damage done by mozzconf.h
#undef compress
#include "mozilla/Compression.h"

// Protocol buffer (generated automatically)
#include "protobuf/LayerScopePacket.pb.h"

namespace mozilla {
namespace layers {

using namespace mozilla::Compression;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla;
using namespace layerscope;

class DebugDataSender;
class DebugGLData;

/*
 * This class handle websocket protocol which included
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
        , mConnected(false)
    { }

    void OpenStream(nsISocketTransport* aTransport);

    bool WriteToStream(void *aPtr, uint32_t aSize);

    // nsIInputStreamCallback
    NS_IMETHODIMP OnInputStreamReady(nsIAsyncInputStream *aStream) override;

private:
    virtual ~LayerScopeWebSocketHandler() { CloseConnection(); }

    void ReadInputStreamData(nsTArray<nsCString>& aProtocolString);

    bool WebSocketHandshake(nsTArray<nsCString>& aProtocolString);

    nsresult HandleSocketMessage(nsIAsyncInputStream *aStream);

    nsresult ProcessInput(uint8_t *aBuffer, uint32_t aCount);

    // Copied from WebsocketChannel, helper function to decode data frame
    void ApplyMask(uint32_t aMask, uint8_t *aData, uint64_t aLen);

    bool HandleDataFrame(uint8_t *aData, uint32_t aSize);

    void CloseConnection();

private:
    nsCOMPtr<nsIOutputStream> mOutputStream;
    nsCOMPtr<nsIAsyncInputStream> mInputStream;
    nsCOMPtr<nsISocketTransport> mTransport;
    SocketStateType mState;
    bool mConnected;
};

NS_IMPL_ISUPPORTS(LayerScopeWebSocketHandler, nsIInputStreamCallback);


/*
 * Manage Websocket connections
 */
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
        MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
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
        return sWebSocketManager;
    }

private:
    static StaticAutoPtr<LayerScopeWebSocketManager> sWebSocketManager;
};

StaticAutoPtr<LayerScopeWebSocketManager> WebSocketHelper::sWebSocketManager;

/*
 * DebugGLData is the base class of
 * 1. DebugGLFrameStatusData (Frame start/end packet)
 * 2. DebugGLColorData (Color data packet)
 * 3. DebugGLTextureData (Texture data packet)
 * 4. DebugGLLayersData (Layers Tree data packet)
 * 5. DebugGLMetaData (Meta data packet)
 */
class DebugGLData: public LinkedListElement<DebugGLData> {
public:
    explicit DebugGLData(Packet::DataType aDataType)
        : mDataType(aDataType)
    { }

    virtual ~DebugGLData() { }

    Packet::DataType GetDataType() const { return mDataType; }

    virtual bool Write() = 0;

    static bool WriteToStream(Packet& aPacket) {
        if (!WebSocketHelper::GetSocketManager())
            return true;

        uint32_t size = aPacket.ByteSize();
        auto data = MakeUnique<uint8_t[]>(size);
        aPacket.SerializeToArray(data.get(), size);
        return WebSocketHelper::GetSocketManager()->WriteAll(data.get(), size);
    }

protected:
    Packet::DataType mDataType;
};

class DebugGLFrameStatusData : public DebugGLData
{
public:
    DebugGLFrameStatusData(Packet::DataType aDataType,
                           int64_t aValue)
        : DebugGLData(aDataType),
          mFrameStamp(aValue)
    { }

    explicit DebugGLFrameStatusData(Packet::DataType aDataType)
        : DebugGLData(aDataType),
          mFrameStamp(0)
    { }

    int64_t GetFrameStamp() const { return mFrameStamp; }

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        FramePacket* fp = packet.mutable_frame();
        fp->set_value(static_cast<uint64_t>(mFrameStamp));

        if (!WriteToStream(packet))
            return false;
        return true;
    }

protected:
    int64_t mFrameStamp;
};

class DebugGLTextureData : public DebugGLData {
public:
    DebugGLTextureData(GLContext* cx,
                       void* layerRef,
                       GLenum target,
                       GLuint name,
                       DataSourceSurface* img)
        : DebugGLData(Packet::TEXTURE),
          mLayerRef(layerRef),
          mTarget(target),
          mName(name),
          mContextAddress(reinterpret_cast<intptr_t>(cx)),
          mDatasize(0)
    {
        // pre-packing
        // DataSourceSurface may have locked buffer,
        // so we should compress now, and then it could
        // be unlocked outside.
        pack(img);
    }

    const void* GetLayerRef() const { return mLayerRef; }
    GLuint GetName() const { return mName; }
    GLenum GetTextureTarget() const { return mTarget; }
    intptr_t GetContextAddress() const { return mContextAddress; }
    uint32_t GetDataSize() const { return mDatasize; }

    virtual bool Write() override {
        if (!WriteToStream(mPacket))
            return false;
        return true;
    }

private:
    void pack(DataSourceSurface* aImage) {
        mPacket.set_type(mDataType);

        TexturePacket* tp = mPacket.mutable_texture();
        tp->set_layerref(reinterpret_cast<uint64_t>(mLayerRef));
        tp->set_name(mName);
        tp->set_target(mTarget);
        tp->set_dataformat(LOCAL_GL_RGBA);
        tp->set_glcontext(static_cast<uint64_t>(mContextAddress));

        if (aImage) {
            tp->set_width(aImage->GetSize().width);
            tp->set_height(aImage->GetSize().height);
            tp->set_stride(aImage->Stride());

            mDatasize = aImage->GetSize().height * aImage->Stride();

            auto compresseddata = MakeUnique<char[]>(LZ4::maxCompressedSize(mDatasize));
            if (compresseddata) {
                int ndatasize = LZ4::compress((char*)aImage->GetData(),
                                              mDatasize,
                                              compresseddata.get());
                if (ndatasize > 0) {
                    mDatasize = ndatasize;
                    tp->set_dataformat((1 << 16 | tp->dataformat()));
                    tp->set_data(compresseddata.get(), mDatasize);
                } else {
                    NS_WARNING("Compress data failed");
                    tp->set_data(aImage->GetData(), mDatasize);
                }
            } else {
                NS_WARNING("Couldn't new compressed data.");
                tp->set_data(aImage->GetData(), mDatasize);
            }
        } else {
            tp->set_width(0);
            tp->set_height(0);
            tp->set_stride(0);
        }
    }

protected:
    void* mLayerRef;
    GLenum mTarget;
    GLuint mName;
    intptr_t mContextAddress;
    uint32_t mDatasize;

    // Packet data
    Packet mPacket;
};

class DebugGLColorData : public DebugGLData {
public:
    DebugGLColorData(void* layerRef,
                     const gfxRGBA& color,
                     int width,
                     int height)
        : DebugGLData(Packet::COLOR),
          mLayerRef(layerRef),
          mColor(color.Packed()),
          mSize(width, height)
    { }

    const void* GetLayerRef() const { return mLayerRef; }
    uint32_t GetColor() const { return mColor; }
    const nsIntSize& GetSize() const { return mSize; }

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        ColorPacket* cp = packet.mutable_color();
        cp->set_layerref(reinterpret_cast<uint64_t>(mLayerRef));
        cp->set_color(mColor);
        cp->set_width(mSize.width);
        cp->set_height(mSize.height);

        if (!WriteToStream(packet))
            return false;
        return true;
    }

protected:
    void* mLayerRef;
    uint32_t mColor;
    nsIntSize mSize;
};

class DebugGLLayersData : public DebugGLData {
public:
    explicit DebugGLLayersData(UniquePtr<Packet> aPacket)
        : DebugGLData(Packet::LAYERS),
          mPacket(Move(aPacket))
    { }

    virtual bool Write() override {
        mPacket->set_type(mDataType);

        if (!WriteToStream(*mPacket))
            return false;
        return true;
    }

protected:
    UniquePtr<Packet> mPacket;
};

class DebugGLMetaData : public DebugGLData
{
public:
    DebugGLMetaData(Packet::DataType aDataType,
                    bool aValue)
        : DebugGLData(aDataType),
          mComposedByHwc(aValue)
    { }

    explicit DebugGLMetaData(Packet::DataType aDataType)
        : DebugGLData(aDataType),
          mComposedByHwc(false)
    { }

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        MetaPacket* mp = packet.mutable_meta();
        mp->set_composedbyhwc(mComposedByHwc);

        if (!WriteToStream(packet))
            return false;
        return true;
    }

protected:
    bool mComposedByHwc;
};


class DebugListener : public nsIServerSocketListener
{
    virtual ~DebugListener() { }

public:

    NS_DECL_THREADSAFE_ISUPPORTS

    DebugListener() { }

    /* nsIServerSocketListener */

    NS_IMETHODIMP OnSocketAccepted(nsIServerSocket *aServ,
                                   nsISocketTransport *aTransport) override
    {
        if (!WebSocketHelper::GetSocketManager())
            return NS_OK;

        printf_stderr("*** LayerScope: Accepted connection\n");
        WebSocketHelper::GetSocketManager()->AddConnection(aTransport);
        return NS_OK;
    }

    NS_IMETHODIMP OnStopListening(nsIServerSocket *aServ,
                                  nsresult aStatus) override
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

    NS_IMETHODIMP Run() override {
        DebugGLData *d;
        nsresult rv = NS_OK;

        while ((d = mList.popFirst()) != nullptr) {
            UniquePtr<DebugGLData> cleaner(d);
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


class CreateServerSocketRunnable : public nsRunnable
{
public:
    NS_IMETHOD Run() {
        WebSocketHelper::CreateServerSocket();
        return NS_OK;
    }
};

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

    static void SetLayersTreeSendable(bool aSet) {sLayersTreeSendable = aSet;}

    static void SetLayersBufferSendable(bool aSet) {sLayersBufferSendable = aSet;}

    static bool GetLayersTreeSendable() {return sLayersTreeSendable;}

    static void ClearTextureIdList();

    static bool IsTextureIdContainsInList(GLuint aTextureId);

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

// Data fields
private:
    static bool sLayersTreeSendable;
    static bool sLayersBufferSendable;
    static std::list<GLuint> sTextureIdList;
};

bool SenderHelper::sLayersTreeSendable = true;
bool SenderHelper::sLayersBufferSendable = true;
std::list<GLuint> SenderHelper::sTextureIdList;


// ----------------------------------------------
// SenderHelper implementation
// ----------------------------------------------
void
SenderHelper::ClearTextureIdList()
{
    std::list<GLuint>::iterator it;
    while (!sTextureIdList.empty()) {
        it = sTextureIdList.begin();
        sTextureIdList.erase(it);
    }
}

bool
SenderHelper::IsTextureIdContainsInList(GLuint aTextureId)
{
    for (std::list<GLuint>::iterator it = sTextureIdList.begin();
         it != sTextureIdList.end(); ++it) {
        if (*it == aTextureId) {
          return true;
        }
    }
    return false;
}

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
        case Layer::TYPE_PAINTED: {
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

    if (!IsTextureIdContainsInList(textureId)) {
      sTextureIdList.push_back(textureId);
      WebSocketHelper::GetSocketManager()->AppendDebugData(
          new DebugGLTextureData(aGLContext, aLayerRef, textureTarget,
                               textureId, img));
    }
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
    if (!sLayersBufferSendable) return;

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
// LayerScopeWebSocketHandler implementation
// ----------------------------------------------
void
LayerScopeWebSocketHandler::OpenStream(nsISocketTransport* aTransport)
{
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

bool
LayerScopeWebSocketHandler::WriteToStream(void *aPtr,
                                          uint32_t aSize)
{
    if (mState == NoHandshake) {
        // Not yet handshake, just return true in case of
        // LayerScope remove this handle
        return true;
    } else if (mState == HandshakeFailed) {
        return false;
    }

    if (!mOutputStream) {
        return false;
    }

    // Generate WebSocket header
    uint8_t wsHeader[10];
    int wsHeaderSize = 0;
    const uint8_t opcode = 0x2;
    wsHeader[0] = 0x80 | (opcode & 0x0f); // FIN + opcode;
    if (aSize <= 125) {
        wsHeaderSize = 2;
        wsHeader[1] = aSize;
    } else if (aSize < 65536) {
        wsHeaderSize = 4;
        wsHeader[1] = 0x7E;
        NetworkEndian::writeUint16(wsHeader + 2, aSize);
    } else {
        wsHeaderSize = 10;
        wsHeader[1] = 0x7F;
        NetworkEndian::writeUint64(wsHeader + 2, aSize);
    }

    // Send WebSocket header
    nsresult rv;
    uint32_t cnt;
    rv = mOutputStream->Write(reinterpret_cast<char*>(wsHeader),
                              wsHeaderSize, &cnt);
    if (NS_FAILED(rv))
        return false;

    uint32_t written = 0;
    while (written < aSize) {
        uint32_t cnt;
        rv = mOutputStream->Write(reinterpret_cast<char*>(aPtr) + written,
                                  aSize - written, &cnt);
        if (NS_FAILED(rv))
            return false;

        written += cnt;
    }

    return true;
}

NS_IMETHODIMP
LayerScopeWebSocketHandler::OnInputStreamReady(nsIAsyncInputStream *aStream)
{
    MOZ_ASSERT(mInputStream);

    if (!mInputStream) {
        return NS_OK;
    }

    if (!mConnected) {
        nsTArray<nsCString> protocolString;
        ReadInputStreamData(protocolString);

        if (WebSocketHandshake(protocolString)) {
            mState = HandshakeSuccess;
            mConnected = true;
            mInputStream->AsyncWait(this, 0, 0, NS_GetCurrentThread());
        } else {
            mState = HandshakeFailed;
        }
        return NS_OK;
    } else {
        return HandleSocketMessage(aStream);
    }
}

void
LayerScopeWebSocketHandler::ReadInputStreamData(nsTArray<nsCString>& aProtocolString)
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

bool
LayerScopeWebSocketHandler::WebSocketHandshake(nsTArray<nsCString>& aProtocolString)
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

    if (!mOutputStream) {
        return false;
    }

    // Client request is valid. Start to generate and send server response.
    nsAutoCString guid("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    nsAutoCString res;
    SHA1Sum sha1;
    nsCString combined(wsKey + guid);
    sha1.update(combined.get(), combined.Length());
    uint8_t digest[SHA1Sum::kHashSize]; // SHA1 digests are 20 bytes long.
    sha1.finish(digest);
    nsCString newString(reinterpret_cast<char*>(digest), SHA1Sum::kHashSize);
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

nsresult
LayerScopeWebSocketHandler::HandleSocketMessage(nsIAsyncInputStream *aStream)
{
    // The reading and parsing of this input stream is customized for layer viewer.
    const uint32_t cPacketSize = 1024;
    char buffer[cPacketSize];
    uint32_t count = 0;
    nsresult rv = NS_OK;

    do {
        rv = mInputStream->Read((char *)buffer, cPacketSize, &count);

        // TODO: combine packets if we have to read more than once

        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
            mInputStream->AsyncWait(this, 0, 0, NS_GetCurrentThread());
            return NS_OK;
        }

        if (NS_FAILED(rv)) {
            break;
        }

        if (count == 0) {
            // NS_BASE_STREAM_CLOSED
            CloseConnection();
            break;
        }

        rv = ProcessInput(reinterpret_cast<uint8_t *>(buffer), count);
    } while (NS_SUCCEEDED(rv) && mInputStream);
    return rv;
}

nsresult
LayerScopeWebSocketHandler::ProcessInput(uint8_t *aBuffer,
                                         uint32_t aCount)
{
    uint32_t avail = aCount;

    // Decode Websocket data frame
    if (avail <= 2) {
        NS_WARNING("Packet size is less than 2 bytes");
        return NS_OK;
    }

    // First byte, data type, only care the opcode
    // rsvBits: aBuffer[0] & 0x70 (0111 0000)
    uint8_t finBit = aBuffer[0] & 0x80; // 1000 0000
    uint8_t opcode = aBuffer[0] & 0x0F; // 0000 1111

    if (!finBit) {
        NS_WARNING("We cannot handle multi-fragments messages in Layerscope websocket parser.");
        return NS_OK;
    }

    // Second byte, data length
    uint8_t maskBit = aBuffer[1] & 0x80; // 1000 0000
    int64_t payloadLength64 = aBuffer[1] & 0x7F; // 0111 1111

    if (!maskBit) {
        NS_WARNING("Client to Server should set the mask bit");
        return NS_OK;
    }

    uint32_t framingLength = 2 + 4; // 4 for masks

    if (payloadLength64 < 126) {
        if (avail < framingLength)
            return NS_OK;
    } else if (payloadLength64 == 126) {
        // 16 bit length field
        framingLength += 2;
        if (avail < framingLength) {
            return NS_OK;
        }

        payloadLength64 = aBuffer[2] << 8 | aBuffer[3];
    } else {
        // 64 bit length
        framingLength += 8;
        if (avail < framingLength) {
            return NS_OK;
        }

        if (aBuffer[2] & 0x80) {
            // Section 4.2 says that the most significant bit MUST be
            // 0. (i.e. this is really a 63 bit value)
            NS_WARNING("High bit of 64 bit length set");
            return NS_ERROR_ILLEGAL_VALUE;
        }

        // copy this in case it is unaligned
        payloadLength64 = NetworkEndian::readInt64(aBuffer + 2);
    }

    uint8_t *payload = aBuffer + framingLength;
    avail -= framingLength;

    uint32_t payloadLength = static_cast<uint32_t>(payloadLength64);
    if (avail < payloadLength) {
        NS_WARNING("Packet size mismatch the payload length");
        return NS_OK;
    }

    // Apply mask
    uint32_t mask = NetworkEndian::readUint32(payload - 4);
    ApplyMask(mask, payload, payloadLength);

    if (opcode == 0x8) {
        // opcode == 0x8 means connection close
        CloseConnection();
        return NS_BASE_STREAM_CLOSED;
    }

    if (!HandleDataFrame(payload, payloadLength)) {
        NS_WARNING("Cannot decode payload data by the protocol buffer");
    }

    return NS_OK;
}

void
LayerScopeWebSocketHandler::ApplyMask(uint32_t aMask,
                                      uint8_t *aData,
                                      uint64_t aLen)
{
    if (!aData || aLen == 0) {
        return;
    }

    // Optimally we want to apply the mask 32 bits at a time,
    // but the buffer might not be alligned. So we first deal with
    // 0 to 3 bytes of preamble individually
    while (aLen && (reinterpret_cast<uintptr_t>(aData) & 3)) {
        *aData ^= aMask >> 24;
        aMask = RotateLeft(aMask, 8);
        aData++;
        aLen--;
    }

    // perform mask on full words of data
    uint32_t *iData = reinterpret_cast<uint32_t *>(aData);
    uint32_t *end = iData + (aLen >> 2);
    NetworkEndian::writeUint32(&aMask, aMask);
    for (; iData < end; iData++) {
        *iData ^= aMask;
    }
    aMask = NetworkEndian::readUint32(&aMask);
    aData = (uint8_t *)iData;
    aLen  = aLen % 4;

    // There maybe up to 3 trailing bytes that need to be dealt with
    // individually
    while (aLen) {
        *aData ^= aMask >> 24;
        aMask = RotateLeft(aMask, 8);
        aData++;
        aLen--;
    }
}

bool
LayerScopeWebSocketHandler::HandleDataFrame(uint8_t *aData,
                                            uint32_t aSize)
{
    // Handle payload data by protocol buffer
    auto p = MakeUnique<CommandPacket>();
    p->ParseFromArray(static_cast<void*>(aData), aSize);

    if (!p->has_type()) {
        MOZ_ASSERT(false, "Protocol buffer decoding failed or cannot recongize it");
        return false;
    }

    switch (p->type()) {
        case CommandPacket::LAYERS_TREE:
            if (p->has_value()) {
                SenderHelper::SetLayersTreeSendable(p->value());
            }
            break;

        case CommandPacket::LAYERS_BUFFER:
            if (p->has_value()) {
                SenderHelper::SetLayersBufferSendable(p->value());
            }
            break;

        case CommandPacket::NO_OP:
        default:
            NS_WARNING("Invalid message type");
            break;
    }
    return true;
}

void
LayerScopeWebSocketHandler::CloseConnection()
{
    WebSocketHelper::GetSocketManager()->CleanDebugData();
    if (mInputStream) {
        mInputStream->AsyncWait(nullptr, 0, 0, nullptr);
        mInputStream = nullptr;
    }
    if (mOutputStream) {
        mOutputStream = nullptr;
    }
    if (mTransport) {
        mTransport->Close(NS_BASE_STREAM_CLOSED);
        mTransport = nullptr;
    }
    mConnected = false;
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
    mServerSocket->Close();
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

    if (NS_IsMainThread()) {
        WebSocketHelper::CreateServerSocket();
    } else {
        // Dispatch creation to main thread, and make sure we
        // dispatch this only once after booting
        static bool dispatched = false;
        if (dispatched) {
            return;
        }
        DebugOnly<nsresult> rv = NS_DispatchToMainThread(new CreateServerSocketRunnable());
        MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to dispatch WebSocket Creation to main thread");
        dispatched = true;
    }
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

void
LayerScope::SendLayerDump(UniquePtr<Packet> aPacket)
{
    // Protect this public function
    if (!CheckSendable() || !SenderHelper::GetLayersTreeSendable()) {
        return;
    }
    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLLayersData(Move(aPacket)));
}

bool
LayerScope::CheckSendable()
{
    // Only compositor threads check LayerScope status
    MOZ_ASSERT(CompositorParent::IsInCompositorThread() || gIsGtest);

    if (!gfxPrefs::LayerScopeEnabled()) {
        return false;
    }
    if (!WebSocketHelper::GetSocketManager()) {
        Init();
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


void
LayerScope::SetHWComposed()
{
    if (CheckSendable()) {
        WebSocketHelper::GetSocketManager()->AppendDebugData(
            new DebugGLMetaData(Packet::META, true));
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
    SenderHelper::ClearTextureIdList();

    if (!LayerScope::CheckSendable()) {
        return;
    }

    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLFrameStatusData(Packet::FRAMESTART, aFrameStamp));
}

void
LayerScopeAutoFrame::EndFrame()
{
    if (!LayerScope::CheckSendable()) {
        return;
    }

    WebSocketHelper::GetSocketManager()->AppendDebugData(
        new DebugGLFrameStatusData(Packet::FRAMEEND));
    WebSocketHelper::GetSocketManager()->DispatchDebugData();
}

} /* layers */
} /* mozilla */
