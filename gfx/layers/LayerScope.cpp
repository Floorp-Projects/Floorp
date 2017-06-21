/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "LayerScope.h"

#include "nsAppRunner.h"
#include "Effects.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"

#include "TexturePoolOGL.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/TextureHostOGL.h"

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
 * Manage Websocket connections
 */
class LayerScopeWebSocketManager {
public:
    LayerScopeWebSocketManager();
    ~LayerScopeWebSocketManager();

    void RemoveAllConnections()
    {
        MOZ_ASSERT(NS_IsMainThread());

        MutexAutoLock lock(mHandlerMutex);
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
        // This funtion can be called in both main thread and compositor thread.
        MutexAutoLock lock(mHandlerMutex);
        return (mHandlers.Length() != 0) ? true : false;
    }

    void AppendDebugData(DebugGLData *aDebugData);
    void CleanDebugData();
    void DispatchDebugData();

private:
    void AddConnection(nsISocketTransport *aTransport)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aTransport);

        MutexAutoLock lock(mHandlerMutex);

        RefPtr<SocketHandler> temp = new SocketHandler();
        temp->OpenStream(aTransport);
        mHandlers.AppendElement(temp.get());
    }

    void RemoveConnection(uint32_t aIndex)
    {
        // TBD: RemoveConnection is executed on the compositor thread and
        // AddConntection is executed on the main thread, which might be
        // a problem if a user disconnect and connect readlly quickly at
        // viewer side.

        // We should dispatch RemoveConnection onto main thead.
        MOZ_ASSERT(aIndex < mHandlers.Length());

        MutexAutoLock lock(mHandlerMutex);
        mHandlers.RemoveElementAt(aIndex);
    }

    friend class SocketListener;
    class SocketListener : public nsIServerSocketListener
    {
    public:
       NS_DECL_THREADSAFE_ISUPPORTS

       SocketListener() { }

       /* nsIServerSocketListener */
       NS_IMETHOD OnSocketAccepted(nsIServerSocket *aServ,
                                   nsISocketTransport *aTransport) override;
       NS_IMETHOD OnStopListening(nsIServerSocket *aServ,
                                  nsresult aStatus) override
       {
           return NS_OK;
       }
    private:
       virtual ~SocketListener() { }
    };

    /*
     * This class handle websocket protocol which included
     * handshake and data frame's header
     */
    class SocketHandler : public nsIInputStreamCallback {
    public:
        NS_DECL_THREADSAFE_ISUPPORTS

        SocketHandler()
            : mState(NoHandshake)
            , mConnected(false)
        { }

        void OpenStream(nsISocketTransport* aTransport);
        bool WriteToStream(void *aPtr, uint32_t aSize);

        // nsIInputStreamCallback
        NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *aStream) override;

    private:
        virtual ~SocketHandler() { CloseConnection(); }

        void ReadInputStreamData(nsTArray<nsCString>& aProtocolString);
        bool WebSocketHandshake(nsTArray<nsCString>& aProtocolString);
        void ApplyMask(uint32_t aMask, uint8_t *aData, uint64_t aLen);
        bool HandleDataFrame(uint8_t *aData, uint32_t aSize);
        void CloseConnection();

        nsresult HandleSocketMessage(nsIAsyncInputStream *aStream);
        nsresult ProcessInput(uint8_t *aBuffer, uint32_t aCount);

    private:
        enum SocketStateType {
            NoHandshake,
            HandshakeSuccess,
            HandshakeFailed
        };
        SocketStateType               mState;

        nsCOMPtr<nsIOutputStream>     mOutputStream;
        nsCOMPtr<nsIAsyncInputStream> mInputStream;
        nsCOMPtr<nsISocketTransport>  mTransport;
        bool                          mConnected;
    };

    nsTArray<RefPtr<SocketHandler> > mHandlers;
    nsCOMPtr<nsIThread>                   mDebugSenderThread;
    RefPtr<DebugDataSender>             mCurrentSender;
    nsCOMPtr<nsIServerSocket>             mServerSocket;

    // Keep mHandlers accessing thread safe.
    Mutex mHandlerMutex;
};

NS_IMPL_ISUPPORTS(LayerScopeWebSocketManager::SocketListener,
                  nsIServerSocketListener);
NS_IMPL_ISUPPORTS(LayerScopeWebSocketManager::SocketHandler,
                  nsIInputStreamCallback);

class DrawSession {
public:
    DrawSession()
      : mOffsetX(0.0)
      , mOffsetY(0.0)
      , mRects(0)
    { }

    float mOffsetX;
    float mOffsetY;
    gfx::Matrix4x4 mMVMatrix;
    size_t mRects;
    gfx::Rect mLayerRects[4];
    gfx::Rect mTextureRects[4];
    std::list<GLuint> mTexIDs;
};

class ContentMonitor {
public:
    using THArray = nsTArray<const TextureHost *>;

    // Notify the content of a TextureHost was changed.
    void SetChangedHost(const TextureHost* host) {
        if (THArray::NoIndex == mChangedHosts.IndexOf(host)) {
            mChangedHosts.AppendElement(host);
        }
    }

    // Clear changed flag of a host.
    void ClearChangedHost(const TextureHost* host) {
        if (THArray::NoIndex != mChangedHosts.IndexOf(host)) {
          mChangedHosts.RemoveElement(host);
        }
    }

    // Return true iff host is a new one or the content of it had been changed.
    bool IsChangedOrNew(const TextureHost* host) {
        if (THArray::NoIndex == mSeenHosts.IndexOf(host)) {
            mSeenHosts.AppendElement(host);
            return true;
        }

        if (decltype(mChangedHosts)::NoIndex != mChangedHosts.IndexOf(host)) {
            return true;
        }

        return false;
    }

    void Empty() {
        mSeenHosts.SetLength(0);
        mChangedHosts.SetLength(0);
    }
private:
    THArray mSeenHosts;
    THArray mChangedHosts;
};

/*
 * Hold all singleton objects used by LayerScope.
 */
class LayerScopeManager
{
public:
    void CreateServerSocket()
    {
        //  WebSocketManager must be created on the main thread.
        if (NS_IsMainThread()) {
            mWebSocketManager = mozilla::MakeUnique<LayerScopeWebSocketManager>();
        } else {
            // Dispatch creation to main thread, and make sure we
            // dispatch this only once after booting
            static bool dispatched = false;
            if (dispatched) {
                return;
            }

            DebugOnly<nsresult> rv =
              NS_DispatchToMainThread(new CreateServerSocketRunnable(this));
            MOZ_ASSERT(NS_SUCCEEDED(rv),
                  "Failed to dispatch WebSocket Creation to main thread");
            dispatched = true;
        }
    }

    void DestroyServerSocket()
    {
        // Destroy Web Server Socket
        if (mWebSocketManager) {
            mWebSocketManager->RemoveAllConnections();
        }
    }

    LayerScopeWebSocketManager* GetSocketManager()
    {
        return mWebSocketManager.get();
    }

    ContentMonitor* GetContentMonitor()
    {
        if (!mContentMonitor.get()) {
            mContentMonitor = mozilla::MakeUnique<ContentMonitor>();
        }

        return mContentMonitor.get();
    }

    void NewDrawSession() {
        mSession = mozilla::MakeUnique<DrawSession>();
    }

    DrawSession& CurrentSession() {
        return *mSession;
    }

    void SetPixelScale(double scale) {
        mScale = scale;
    }

    double GetPixelScale() const {
        return mScale;
    }

    LayerScopeManager()
        : mScale(1.0)
    {
    }
private:
    friend class CreateServerSocketRunnable;
    class CreateServerSocketRunnable : public Runnable
    {
    public:
        explicit CreateServerSocketRunnable(LayerScopeManager *aLayerScopeManager)
            : mLayerScopeManager(aLayerScopeManager)
        {
        }
        NS_IMETHOD Run() override {
            mLayerScopeManager->mWebSocketManager =
                mozilla::MakeUnique<LayerScopeWebSocketManager>();
            return NS_OK;
        }
    private:
        LayerScopeManager* mLayerScopeManager;
    };

    mozilla::UniquePtr<LayerScopeWebSocketManager> mWebSocketManager;
    mozilla::UniquePtr<DrawSession> mSession;
    mozilla::UniquePtr<ContentMonitor> mContentMonitor;
    double mScale;
};

LayerScopeManager gLayerScopeManager;

/*
 * The static helper functions that set data into the packet
 * 1. DumpRect
 * 2. DumpFilter
 */
template<typename T>
static void DumpRect(T* aPacketRect, const Rect& aRect)
{
    aPacketRect->set_x(aRect.x);
    aPacketRect->set_y(aRect.y);
    aPacketRect->set_w(aRect.width);
    aPacketRect->set_h(aRect.height);
}

static void DumpFilter(TexturePacket* aTexturePacket,
                       const SamplingFilter aSamplingFilter)
{
    switch (aSamplingFilter) {
        case SamplingFilter::GOOD:
            aTexturePacket->set_mfilter(TexturePacket::GOOD);
            break;
        case SamplingFilter::LINEAR:
            aTexturePacket->set_mfilter(TexturePacket::LINEAR);
            break;
        case SamplingFilter::POINT:
            aTexturePacket->set_mfilter(TexturePacket::POINT);
            break;
        default:
            MOZ_ASSERT(false, "Can't dump unexpected mSamplingFilter to texture packet!");
            break;
    }
}

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

    virtual bool Write() = 0;

protected:
    static bool WriteToStream(Packet& aPacket) {
        if (!gLayerScopeManager.GetSocketManager())
            return true;

        uint32_t size = aPacket.ByteSize();
        auto data = MakeUnique<uint8_t[]>(size);
        aPacket.SerializeToArray(data.get(), size);
        return gLayerScopeManager.GetSocketManager()->WriteAll(data.get(), size);
    }

    Packet::DataType mDataType;
};

class DebugGLFrameStatusData final: public DebugGLData
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

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        FramePacket* fp = packet.mutable_frame();
        fp->set_value(static_cast<uint64_t>(mFrameStamp));

        fp->set_scale(gLayerScopeManager.GetPixelScale());

        return WriteToStream(packet);
    }

protected:
    int64_t mFrameStamp;
};

class DebugGLTextureData final: public DebugGLData {
public:
    DebugGLTextureData(GLContext* cx,
                       void* layerRef,
                       GLenum target,
                       GLuint name,
                       DataSourceSurface* img,
                       bool aIsMask,
                       UniquePtr<Packet> aPacket)
        : DebugGLData(Packet::TEXTURE),
          mLayerRef(reinterpret_cast<uint64_t>(layerRef)),
          mTarget(target),
          mName(name),
          mContextAddress(reinterpret_cast<intptr_t>(cx)),
          mDatasize(0),
          mIsMask(aIsMask),
          mPacket(Move(aPacket))
    {
        // pre-packing
        // DataSourceSurface may have locked buffer,
        // so we should compress now, and then it could
        // be unlocked outside.
        pack(img);
    }

    virtual bool Write() override {
        return WriteToStream(*mPacket);
    }

private:
    void pack(DataSourceSurface* aImage) {
        mPacket->set_type(mDataType);

        TexturePacket* tp = mPacket->mutable_texture();
        tp->set_layerref(mLayerRef);
        tp->set_name(mName);
        tp->set_target(mTarget);
        tp->set_dataformat(LOCAL_GL_RGBA);
        tp->set_glcontext(static_cast<uint64_t>(mContextAddress));
        tp->set_ismask(mIsMask);

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
    uint64_t mLayerRef;
    GLenum mTarget;
    GLuint mName;
    intptr_t mContextAddress;
    uint32_t mDatasize;
    bool mIsMask;

    // Packet data
    UniquePtr<Packet> mPacket;
};

class DebugGLColorData final: public DebugGLData {
public:
    DebugGLColorData(void* layerRef,
                     const Color& color,
                     int width,
                     int height)
        : DebugGLData(Packet::COLOR),
          mLayerRef(reinterpret_cast<uint64_t>(layerRef)),
          mColor(color.ToABGR()),
          mSize(width, height)
    { }

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        ColorPacket* cp = packet.mutable_color();
        cp->set_layerref(mLayerRef);
        cp->set_color(mColor);
        cp->set_width(mSize.width);
        cp->set_height(mSize.height);

        return WriteToStream(packet);
    }

protected:
    uint64_t mLayerRef;
    uint32_t mColor;
    IntSize mSize;
};

class DebugGLLayersData final: public DebugGLData {
public:
    explicit DebugGLLayersData(UniquePtr<Packet> aPacket)
        : DebugGLData(Packet::LAYERS),
          mPacket(Move(aPacket))
    { }

    virtual bool Write() override {
        mPacket->set_type(mDataType);
        return WriteToStream(*mPacket);
    }

protected:
    UniquePtr<Packet> mPacket;
};

class DebugGLMetaData final: public DebugGLData
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

        return WriteToStream(packet);
    }

protected:
    bool mComposedByHwc;
};

class DebugGLDrawData final: public DebugGLData {
public:
    DebugGLDrawData(float aOffsetX,
                    float aOffsetY,
                    const gfx::Matrix4x4& aMVMatrix,
                    size_t aRects,
                    const gfx::Rect* aLayerRects,
                    const gfx::Rect* aTextureRects,
                    const std::list<GLuint> aTexIDs,
                    void* aLayerRef)
        : DebugGLData(Packet::DRAW),
          mOffsetX(aOffsetX),
          mOffsetY(aOffsetY),
          mMVMatrix(aMVMatrix),
          mRects(aRects),
          mTexIDs(aTexIDs),
          mLayerRef(reinterpret_cast<uint64_t>(aLayerRef))
    {
        for (size_t i = 0; i < mRects; i++){
            mLayerRects[i] = aLayerRects[i];
            mTextureRects[i] = aTextureRects[i];
        }
    }

    virtual bool Write() override {
        Packet packet;
        packet.set_type(mDataType);

        DrawPacket* dp = packet.mutable_draw();
        dp->set_layerref(mLayerRef);

        dp->set_offsetx(mOffsetX);
        dp->set_offsety(mOffsetY);

        auto element = reinterpret_cast<Float *>(&mMVMatrix);
        for (int i = 0; i < 16; i++) {
          dp->add_mvmatrix(*element++);
        }
        dp->set_totalrects(mRects);

        MOZ_ASSERT(mRects > 0 && mRects < 4);
        for (size_t i = 0; i < mRects; i++) {
            // Vertex
            DumpRect(dp->add_layerrect(), mLayerRects[i]);
            // UV
            DumpRect(dp->add_texturerect(), mTextureRects[i]);
        }

        for (GLuint texId: mTexIDs) {
            dp->add_texids(texId);
        }

        return WriteToStream(packet);
    }

protected:
    float mOffsetX;
    float mOffsetY;
    gfx::Matrix4x4 mMVMatrix;
    size_t mRects;
    gfx::Rect mLayerRects[4];
    gfx::Rect mTextureRects[4];
    std::list<GLuint> mTexIDs;
    uint64_t mLayerRef;
};

class DebugDataSender
{
public:
   NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DebugDataSender)

    // Append a DebugData into mList on mThread
    class AppendTask: public nsIRunnable
    {
    public:
        NS_DECL_THREADSAFE_ISUPPORTS

        AppendTask(DebugDataSender *host, DebugGLData *d)
            : mData(d),
              mHost(host)
        {  }

        NS_IMETHOD Run() override {
            mHost->mList.insertBack(mData);
            return NS_OK;
        }

    private:
        virtual ~AppendTask() { }

        DebugGLData *mData;
        // Keep a strong reference to DebugDataSender to prevent this object
        // accessing mHost on mThread, when it's been destroyed on the main
        // thread.
        RefPtr<DebugDataSender> mHost;
    };

    // Clear all DebugData in mList on mThead.
    class ClearTask: public nsIRunnable
    {
    public:
        NS_DECL_THREADSAFE_ISUPPORTS
        explicit ClearTask(DebugDataSender *host)
            : mHost(host)
        {  }

        NS_IMETHOD Run() override {
            mHost->RemoveData();
            return NS_OK;
        }

    private:
        virtual ~ClearTask() { }

        RefPtr<DebugDataSender> mHost;
    };

    // Send all DebugData in mList via websocket, and then, clean up
    // mList on mThread.
    class SendTask: public nsIRunnable
    {
    public:
        NS_DECL_THREADSAFE_ISUPPORTS

        explicit SendTask(DebugDataSender *host)
            : mHost(host)
        {  }

        NS_IMETHOD Run() override {
            // Sendout all appended debug data.
            DebugGLData *d = nullptr;
            while ((d = mHost->mList.popFirst()) != nullptr) {
                UniquePtr<DebugGLData> cleaner(d);
                if (!d->Write()) {
                    gLayerScopeManager.DestroyServerSocket();
                    break;
                }
            }

            // Cleanup.
            mHost->RemoveData();
            return NS_OK;
        }
    private:
        virtual ~SendTask() { }

        RefPtr<DebugDataSender> mHost;
    };

    explicit DebugDataSender(nsIThread *thread)
        : mThread(thread)
    {  }

    void Append(DebugGLData *d) {
        mThread->Dispatch(new AppendTask(this, d), NS_DISPATCH_NORMAL);
    }

    void Cleanup() {
        mThread->Dispatch(new ClearTask(this), NS_DISPATCH_NORMAL);
    }

    void Send() {
        mThread->Dispatch(new SendTask(this), NS_DISPATCH_NORMAL);
    }

protected:
    virtual ~DebugDataSender() {}
    void RemoveData() {
        MOZ_ASSERT(mThread->SerialEventTarget()->IsOnCurrentThread());
        if (mList.isEmpty())
            return;

        DebugGLData *d;
        while ((d = mList.popFirst()) != nullptr)
            delete d;
    }

    // We can only modify or aceess mList on mThread.
    LinkedList<DebugGLData> mList;
    nsCOMPtr<nsIThread>     mThread;
};

NS_IMPL_ISUPPORTS(DebugDataSender::AppendTask, nsIRunnable);
NS_IMPL_ISUPPORTS(DebugDataSender::ClearTask, nsIRunnable);
NS_IMPL_ISUPPORTS(DebugDataSender::SendTask, nsIRunnable);


/*
 * LayerScope SendXXX Structure
 * 1. SendLayer
 * 2. SendEffectChain
 *   1. SendTexturedEffect
 *      -> SendTextureSource
 *   2. SendMaskEffect
 *      -> SendTextureSource
 *   3. SendYCbCrEffect
 *      -> SendTextureSource
 *   4. SendColor
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

    static void ClearSentTextureIds();

// Sender private functions
private:
    static void SendColor(void* aLayerRef,
                          const Color& aColor,
                          int aWidth,
                          int aHeight);
    static void SendTextureSource(GLContext* aGLContext,
                                  void* aLayerRef,
                                  TextureSourceOGL* aSource,
                                  bool aFlipY,
                                  bool aIsMask,
                                  UniquePtr<Packet> aPacket);
    static void SetAndSendTexture(GLContext* aGLContext,
                                  void* aLayerRef,
                                  TextureSourceOGL* aSource,
                                  const TexturedEffect* aEffect);
    static void SendTexturedEffect(GLContext* aGLContext,
                                   void* aLayerRef,
                                   const TexturedEffect* aEffect);
    static void SendMaskEffect(GLContext* aGLContext,
                                   void* aLayerRef,
                                   const EffectMask* aEffect);
    static void SendYCbCrEffect(GLContext* aGLContext,
                                void* aLayerRef,
                                const EffectYCbCr* aEffect);
    static GLuint GetTextureID(GLContext* aGLContext,
                               TextureSourceOGL* aSource);
    static bool HasTextureIdBeenSent(GLuint aTextureId);
// Data fields
private:
    static bool sLayersTreeSendable;
    static bool sLayersBufferSendable;
    static std::vector<GLuint> sSentTextureIds;
};

bool SenderHelper::sLayersTreeSendable = true;
bool SenderHelper::sLayersBufferSendable = true;
std::vector<GLuint> SenderHelper::sSentTextureIds;


// ----------------------------------------------
// SenderHelper implementation
// ----------------------------------------------
void
SenderHelper::ClearSentTextureIds()
{
    sSentTextureIds.clear();
}

bool
SenderHelper::HasTextureIdBeenSent(GLuint aTextureId)
{
    return std::find(sSentTextureIds.begin(), sSentTextureIds.end(), aTextureId) != sSentTextureIds.end();
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

            LayerScope::DrawBegin();
            LayerScope::DrawEnd(nullptr, effect, aWidth, aHeight);
            break;
        }
        case Layer::TYPE_IMAGE:
        case Layer::TYPE_CANVAS:
        case Layer::TYPE_PAINTED: {
            // Get CompositableHost and Compositor
            CompositableHost* compHost = aLayer->GetCompositableHost();
            TextureSourceProvider* provider = compHost->GetTextureSourceProvider();
            Compositor* comp = provider->AsCompositor();
            // Send EffectChain only for CompositorOGL
            if (LayersBackend::LAYERS_OPENGL == comp->GetBackendType()) {
                CompositorOGL* compOGL = comp->AsCompositorOGL();
                EffectChain effect;
                // Generate primary effect (lock and gen)
                AutoLockCompositableHost lock(compHost);
                aLayer->GenEffectChain(effect);

                LayerScope::DrawBegin();
                LayerScope::DrawEnd(compOGL->gl(), effect, aWidth, aHeight);
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
                        const Color& aColor,
                        int aWidth,
                        int aHeight)
{
    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLColorData(aLayerRef, aColor, aWidth, aHeight));
}

GLuint
SenderHelper::GetTextureID(GLContext* aGLContext,
                           TextureSourceOGL* aSource) {
    GLenum textureTarget = aSource->GetTextureTarget();
    aSource->BindTexture(LOCAL_GL_TEXTURE0, gfx::SamplingFilter::LINEAR);

    GLuint texID = 0;
    // This is horrid hack. It assumes that aGLContext matches the context
    // aSource has bound to.
    if (textureTarget == LOCAL_GL_TEXTURE_2D) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &texID);
    } else if (textureTarget == LOCAL_GL_TEXTURE_EXTERNAL) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_EXTERNAL, &texID);
    } else if (textureTarget == LOCAL_GL_TEXTURE_RECTANGLE) {
        aGLContext->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_RECTANGLE, &texID);
    }

    return texID;
}

void
SenderHelper::SendTextureSource(GLContext* aGLContext,
                                void* aLayerRef,
                                TextureSourceOGL* aSource,
                                bool aFlipY,
                                bool aIsMask,
                                UniquePtr<Packet> aPacket)
{
    MOZ_ASSERT(aGLContext);
    if (!aGLContext) {
        return;
    }
    GLuint texID = GetTextureID(aGLContext, aSource);
    if (HasTextureIdBeenSent(texID)) {
        return;
    }

    GLenum textureTarget = aSource->GetTextureTarget();
    ShaderConfigOGL config = ShaderConfigFromTargetAndFormat(textureTarget,
                                                             aSource->GetFormat());
    int shaderConfig = config.mFeatures;

    gfx::IntSize size = aSource->GetSize();

    // By sending 0 to ReadTextureImage rely upon aSource->BindTexture binding
    // texture correctly. texID is used for tracking in DebugGLTextureData.
    RefPtr<DataSourceSurface> img =
        aGLContext->ReadTexImageHelper()->ReadTexImage(0, textureTarget,
                                                         size,
                                                         shaderConfig, aFlipY);
    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLTextureData(aGLContext, aLayerRef, textureTarget,
                               texID, img, aIsMask, Move(aPacket)));

    sSentTextureIds.push_back(texID);
    gLayerScopeManager.CurrentSession().mTexIDs.push_back(texID);

}

void
SenderHelper::SetAndSendTexture(GLContext* aGLContext,
                                void* aLayerRef,
                                TextureSourceOGL* aSource,
                                const TexturedEffect* aEffect)
{
    // Expose packet creation here, so we could dump primary texture effect attributes.
    auto packet = MakeUnique<layerscope::Packet>();
    layerscope::TexturePacket* texturePacket = packet->mutable_texture();
    texturePacket->set_mpremultiplied(aEffect->mPremultiplied);
    DumpFilter(texturePacket, aEffect->mSamplingFilter);
    DumpRect(texturePacket->mutable_mtexturecoords(), aEffect->mTextureCoords);
    SendTextureSource(aGLContext, aLayerRef, aSource, false, false, Move(packet));
}

void
SenderHelper::SendTexturedEffect(GLContext* aGLContext,
                                 void* aLayerRef,
                                 const TexturedEffect* aEffect)
{
    TextureSourceOGL* source = aEffect->mTexture->AsSourceOGL();
    if (!source) {
        return;
    }

    // Fallback texture sending path.
    SetAndSendTexture(aGLContext, aLayerRef, source, aEffect);
}

void
SenderHelper::SendMaskEffect(GLContext* aGLContext,
                                 void* aLayerRef,
                                 const EffectMask* aEffect)
{
    TextureSourceOGL* source = aEffect->mMaskTexture->AsSourceOGL();
    if (!source) {
        return;
    }

    // Expose packet creation here, so we could dump secondary mask effect attributes.
    auto packet = MakeUnique<layerscope::Packet>();
    TexturePacket::EffectMask* mask = packet->mutable_texture()->mutable_mask();
    mask->mutable_msize()->set_w(aEffect->mSize.width);
    mask->mutable_msize()->set_h(aEffect->mSize.height);
    auto element = reinterpret_cast<const Float *>(&(aEffect->mMaskTransform));
    for (int i = 0; i < 16; i++) {
        mask->mutable_mmasktransform()->add_m(*element++);
    }

    SendTextureSource(aGLContext, aLayerRef, source, false, true, Move(packet));
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
    TextureSourceOGL *sources[] = {
        sourceYCbCr->GetSubSource(Y)->AsSourceOGL(),
        sourceYCbCr->GetSubSource(Cb)->AsSourceOGL(),
        sourceYCbCr->GetSubSource(Cr)->AsSourceOGL()
    };

    for (auto source: sources) {
        SetAndSendTexture(aGLContext, aLayerRef, source, aEffect);
    }
}

void
SenderHelper::SendEffectChain(GLContext* aGLContext,
                              const EffectChain& aEffectChain,
                              int aWidth,
                              int aHeight)
{
    if (!sLayersBufferSendable) return;

    const Effect* primaryEffect = aEffectChain.mPrimaryEffect;
    MOZ_ASSERT(primaryEffect);

    if (!primaryEffect) {
      return;
    }

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
            SendColor(aEffectChain.mLayerRef, solidColorEffect->mColor,
                      aWidth, aHeight);
            break;
        }
        case EffectTypes::COMPONENT_ALPHA:
        case EffectTypes::RENDER_TARGET:
        default:
            break;
    }

    if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
        const EffectMask* effectMask =
            static_cast<const EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
        SendMaskEffect(aGLContext, aEffectChain.mLayerRef, effectMask);
    }
}

void
LayerScope::ContentChanged(TextureHost *host)
{
    if (!CheckSendable()) {
      return;
    }

    gLayerScopeManager.GetContentMonitor()->SetChangedHost(host);
}

// ----------------------------------------------
// SocketHandler implementation
// ----------------------------------------------
void
LayerScopeWebSocketManager::SocketHandler::OpenStream(nsISocketTransport* aTransport)
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
    mInputStream->AsyncWait(this, 0, 0, GetCurrentThreadEventTarget());
}

bool
LayerScopeWebSocketManager::SocketHandler::WriteToStream(void *aPtr,
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
LayerScopeWebSocketManager::SocketHandler::OnInputStreamReady(nsIAsyncInputStream *aStream)
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
            mInputStream->AsyncWait(this, 0, 0, GetCurrentThreadEventTarget());
        } else {
            mState = HandshakeFailed;
        }
        return NS_OK;
    } else {
        return HandleSocketMessage(aStream);
    }
}

void
LayerScopeWebSocketManager::SocketHandler::ReadInputStreamData(nsTArray<nsCString>& aProtocolString)
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
LayerScopeWebSocketManager::SocketHandler::WebSocketHandshake(nsTArray<nsCString>& aProtocolString)
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
    rv = Base64Encode(newString, res);
    if (NS_FAILED(rv)) {
        return false;
    }
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
LayerScopeWebSocketManager::SocketHandler::HandleSocketMessage(nsIAsyncInputStream *aStream)
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
            mInputStream->AsyncWait(this, 0, 0, GetCurrentThreadEventTarget());
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
LayerScopeWebSocketManager::SocketHandler::ProcessInput(uint8_t *aBuffer,
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
LayerScopeWebSocketManager::SocketHandler::ApplyMask(uint32_t aMask,
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
LayerScopeWebSocketManager::SocketHandler::HandleDataFrame(uint8_t *aData,
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
LayerScopeWebSocketManager::SocketHandler::CloseConnection()
{
    gLayerScopeManager.GetSocketManager()->CleanDebugData();
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
    : mHandlerMutex("LayerScopeWebSocketManager::mHandlerMutex")
{
    NS_NewNamedThread("LayerScope", getter_AddRefs(mDebugSenderThread));

    mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
    int port = gfxPrefs::LayerScopePort();
    mServerSocket->Init(port, false, -1);
    mServerSocket->AsyncListen(new SocketListener);
}

LayerScopeWebSocketManager::~LayerScopeWebSocketManager()
{
    mServerSocket->Close();
}

void
LayerScopeWebSocketManager::AppendDebugData(DebugGLData *aDebugData)
{
    if (!mCurrentSender) {
        mCurrentSender = new DebugDataSender(mDebugSenderThread);
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
    MOZ_ASSERT(mCurrentSender.get() != nullptr);

    mCurrentSender->Send();
    mCurrentSender = nullptr;
}

NS_IMETHODIMP LayerScopeWebSocketManager::SocketListener::OnSocketAccepted(
                                     nsIServerSocket *aServ,
                                     nsISocketTransport *aTransport)
{
    if (!gLayerScopeManager.GetSocketManager())
        return NS_OK;

    printf_stderr("*** LayerScope: Accepted connection\n");
    gLayerScopeManager.GetSocketManager()->AddConnection(aTransport);
    gLayerScopeManager.GetContentMonitor()->Empty();
    return NS_OK;
}

// ----------------------------------------------
// LayerScope implementation
// ----------------------------------------------
/*static*/
void
LayerScope::Init()
{
    if (!gfxPrefs::LayerScopeEnabled() || XRE_IsGPUProcess()) {
        return;
    }

    gLayerScopeManager.CreateServerSocket();
}

/*static*/
void
LayerScope::DrawBegin()
{
    if (!CheckSendable()) {
        return;
    }

    gLayerScopeManager.NewDrawSession();
}

/*static*/
void
LayerScope::SetRenderOffset(float aX, float aY)
{
    if (!CheckSendable()) {
        return;
    }

    gLayerScopeManager.CurrentSession().mOffsetX = aX;
    gLayerScopeManager.CurrentSession().mOffsetY = aY;
}

/*static*/
void
LayerScope::SetLayerTransform(const gfx::Matrix4x4& aMatrix)
{
    if (!CheckSendable()) {
        return;
    }

    gLayerScopeManager.CurrentSession().mMVMatrix = aMatrix;
}

/*static*/
void
LayerScope::SetDrawRects(size_t aRects,
                         const gfx::Rect* aLayerRects,
                         const gfx::Rect* aTextureRects)
{
    if (!CheckSendable()) {
        return;
    }

    MOZ_ASSERT(aRects > 0 && aRects <= 4);
    MOZ_ASSERT(aLayerRects);

    gLayerScopeManager.CurrentSession().mRects = aRects;

    for (size_t i = 0; i < aRects; i++){
        gLayerScopeManager.CurrentSession().mLayerRects[i] = aLayerRects[i];
        gLayerScopeManager.CurrentSession().mTextureRects[i] = aTextureRects[i];
    }
}

/*static*/
void
LayerScope::DrawEnd(gl::GLContext* aGLContext,
                    const EffectChain& aEffectChain,
                    int aWidth,
                    int aHeight)
{
    // Protect this public function
    if (!CheckSendable()) {
        return;
    }

    // 1. Send textures.
    SenderHelper::SendEffectChain(aGLContext, aEffectChain, aWidth, aHeight);

    // 2. Send parameters of draw call, such as uniforms and attributes of
    // vertex adnd fragment shader.
    DrawSession& draws = gLayerScopeManager.CurrentSession();
    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLDrawData(draws.mOffsetX, draws.mOffsetY,
                            draws.mMVMatrix, draws.mRects,
                            draws.mLayerRects,
                            draws.mTextureRects,
                            draws.mTexIDs,
                            aEffectChain.mLayerRef));

}

/*static*/
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

/*static*/
void
LayerScope::SendLayerDump(UniquePtr<Packet> aPacket)
{
    // Protect this public function
    if (!CheckSendable() || !SenderHelper::GetLayersTreeSendable()) {
        return;
    }
    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLLayersData(Move(aPacket)));
}

/*static*/
bool
LayerScope::CheckSendable()
{
    // Only compositor threads check LayerScope status
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread() || gIsGtest);

    if (!gfxPrefs::LayerScopeEnabled()) {
        return false;
    }
    if (!gLayerScopeManager.GetSocketManager()) {
        Init();
        return false;
    }
    if (!gLayerScopeManager.GetSocketManager()->IsConnected()) {
        return false;
    }
    return true;
}

/*static*/
void
LayerScope::CleanLayer()
{
    if (CheckSendable()) {
        gLayerScopeManager.GetSocketManager()->CleanDebugData();
    }
}

/*static*/
void
LayerScope::SetHWComposed()
{
    if (CheckSendable()) {
        gLayerScopeManager.GetSocketManager()->AppendDebugData(
            new DebugGLMetaData(Packet::META, true));
    }
}

/*static*/
void
LayerScope::SetPixelScale(double devPixelsPerCSSPixel)
{
    gLayerScopeManager.SetPixelScale(devPixelsPerCSSPixel);
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
    SenderHelper::ClearSentTextureIds();

    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLFrameStatusData(Packet::FRAMESTART, aFrameStamp));
}

void
LayerScopeAutoFrame::EndFrame()
{
    if (!LayerScope::CheckSendable()) {
        return;
    }

    gLayerScopeManager.GetSocketManager()->AppendDebugData(
        new DebugGLFrameStatusData(Packet::FRAMEEND));
    gLayerScopeManager.GetSocketManager()->DispatchDebugData();
}

} // namespace layers
} // namespace mozilla
