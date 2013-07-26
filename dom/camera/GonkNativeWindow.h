/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_CAMERA_GONKNATIVEWINDOW_H
#define DOM_CAMERA_GONKNATIVEWINDOW_H

#include <stdint.h>
#include <sys/types.h>

#include <gui/ISurfaceTexture.h>
#include <ui/egl/android_natives.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/threads.h>

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "GrallocImages.h"
#include "CameraCommon.h"

namespace android {

// The user of GonkNativeWindow who wants to receive notification of
// new frames should implement this interface.
class GonkNativeWindowNewFrameCallback {
public:
    virtual void OnNewFrame() = 0;
};

class GonkNativeWindow : public BnSurfaceTexture
{
    friend class GonkNativeWindowClient;

    typedef mozilla::layers::SurfaceDescriptor SurfaceDescriptor;
    typedef mozilla::layers::GraphicBufferLocked GraphicBufferLocked;

public:
    enum { MIN_UNDEQUEUED_BUFFERS = 2 };
    enum { MIN_BUFFER_SLOTS = MIN_UNDEQUEUED_BUFFERS };
    enum { NUM_BUFFER_SLOTS = 32 };
    enum { NO_CONNECTED_API = 0 };
    enum { NATIVE_WINDOW_SET_BUFFERS_SIZE = 0x10000000 };

    GonkNativeWindow();
    ~GonkNativeWindow(); // this class cannot be overloaded

    // Get next frame from the queue and mark it as RENDERING, caller
    // owns the returned buffer.
    already_AddRefed<GraphicBufferLocked> getCurrentBuffer();

    // Return the buffer to the queue and mark it as FREE. After that
    // the buffer is useable again for the decoder.
    bool returnBuffer(uint32_t index, uint32_t generation);

    // setBufferCount updates the number of available buffer slots.  After
    // calling this all buffer slots are owned by the GonkNativeWindow object
    // (i.e. they are not owned by the client).
    virtual status_t setBufferCount(int bufferCount);

    // requestBuffer requests a new buffer for the given index.
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);

    // dequeueBuffer gets the next buffer slot index for the client to use. If a
    // buffer slot is available then that slot index is written to the location
    // pointed to by the buf argument and a status of OK is returned.  If no
    // slot is available then a status of -EBUSY is returned and buf is
    // unmodified.
    virtual status_t dequeueBuffer(int *buf, uint32_t width, uint32_t height,
            uint32_t format, uint32_t usage);

    // queueBuffer returns a filled buffer to the GonkNativeWindow. In addition,
    // a timestamp must be provided for the buffer. The timestamp is in
    // nanoseconds, and must be monotonically increasing. Its other semantics
    // (zero point, etc) are client-dependent and should be documented by the
    // client.
    virtual status_t queueBuffer(int buf, int64_t timestamp,
            uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform);
    virtual void cancelBuffer(int buf);
    virtual status_t setCrop(const Rect& reg);
    virtual status_t setTransform(uint32_t transform);
    virtual status_t setScalingMode(int mode);

    virtual int query(int what, int* value);

    // Qcom specific function
    virtual int performQcomOperation(int operation, int arg1, int arg2, int arg3) {
        return OK;
    }

    // GonkNativeWindow do not implement the function and always works in
    // synchronous mode.
    virtual status_t setSynchronousMode(bool enabled);

    // connect attempts to connect a client API to the GonkNativeWindow.
    //
    // This method will fail if the connect was previously called on the
    // GonkNativeWindow and no corresponding disconnect call was made.
    virtual status_t connect(int api,
            uint32_t* outWidth, uint32_t* outHeight, uint32_t* outTransform);

    // disconnect attempts to disconnect a client API from the GonkNativeWindow.
    // This method will fail if the the GonkNativeWindow is not currently
    // connected to the specified client API.
    virtual status_t disconnect(int api);

    void setNewFrameCallback(GonkNativeWindowNewFrameCallback* aCallback);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // requestBuffers when a with and height of zero is requested.
    // A call to setDefaultBufferSize() may trigger requestBuffers() to
    // be called from the client.
    status_t setDefaultBufferSize(uint32_t width, uint32_t height);

    // abandon frees all the buffers and puts the GonkNativeWindow into the
    // 'abandoned' state.  Once put in this state the GonkNativeWindow can never
    // leave it.  When in the 'abandoned' state, all methods of the
    // ISurfaceTexture interface will fail with the NO_INIT error.
    //
    // Note that while calling this method causes all the buffers to be freed
    // from the perspective of the the GonkNativeWindow, if there are additional
    // references on the buffers (e.g. if a buffer is referenced by a client)
    // then those buffer will remain allocated.
    void abandon();

    SurfaceDescriptor *getSurfaceDescriptorFromBuffer(ANativeWindowBuffer* buffer);

protected:

    // freeAllBuffersLocked frees the resources (both GraphicBuffer and
    // EGLImage) for all slots by removing them from the slots and appending
    // then to the freeList.  This must be called with mMutex locked.
    void freeAllBuffersLocked(nsTArray<SurfaceDescriptor>& freeList);

    // releaseBufferFreeListUnlocked releases the resources in the freeList;
    // this must be called with mMutex unlocked.
    void releaseBufferFreeListUnlocked(nsTArray<SurfaceDescriptor>& freeList);

    // clearRenderingStateBuffersLocked clear the resources in RENDERING state;
    // But do not destroy the gralloc buffer. It is still in the video stream
    // awaiting rendering.
    // this must be called with mMutex locked.
    void clearRenderingStateBuffersLocked();

private:
    void init();

    int getSlotFromBufferLocked(android_native_buffer_t* buffer) const;

    enum { INVALID_BUFFER_SLOT = -1 };

    struct BufferSlot {

        BufferSlot()
            : mBufferState(BufferSlot::FREE),
              mTimestamp(0),
              mFrameNumber(0){
        }

        // mGraphicBuffer points to the buffer allocated for this slot or is NULL
        // if no buffer has been allocated.
        sp<GraphicBuffer> mGraphicBuffer;

        // mSurfaceDescriptor is the token to remotely allocated GraphicBuffer.
        SurfaceDescriptor mSurfaceDescriptor;

        // BufferState represents the different states in which a buffer slot
        // can be.
        enum BufferState {
            // FREE indicates that the buffer is not currently being used and
            // will not be used in the future until it gets dequeued and
            // subsequently queued by the client.
            FREE = 0,

            // DEQUEUED indicates that the buffer has been dequeued by the
            // client, but has not yet been queued or canceled. The buffer is
            // considered 'owned' by the client, and the server should not use
            // it for anything.
            //
            // Note that when in synchronous-mode (mSynchronousMode == true),
            // the buffer that's currently attached to the texture may be
            // dequeued by the client.  That means that the current buffer can
            // be in either the DEQUEUED or QUEUED state.  In asynchronous mode,
            // however, the current buffer is always in the QUEUED state.
            DEQUEUED = 1,

            // QUEUED indicates that the buffer has been queued by the client,
            // and has not since been made available for the client to dequeue.
            // Attaching the buffer to the texture does NOT transition the
            // buffer away from the QUEUED state. However, in Synchronous mode
            // the current buffer may be dequeued by the client under some
            // circumstances. See the note about the current buffer in the
            // documentation for DEQUEUED.
            QUEUED = 2,

            // RENDERING indicates that the buffer has been sent to
            // the compositor, and has not yet available for the
            // client to dequeue. When the compositor has finished its
            // job, the buffer will be returned to FREE state.
            RENDERING = 3,
        };

        // mBufferState is the current state of this buffer slot.
        BufferState mBufferState;

        // mRequestBufferCalled is used for validating that the client did
        // call requestBuffer() when told to do so. Technically this is not
        // needed but useful for debugging and catching client bugs.
        bool mRequestBufferCalled;

        // mTimestamp is the current timestamp for this buffer slot. This gets
        // to set by queueBuffer each time this slot is queued.
        int64_t mTimestamp;

        // mFrameNumber is the number of the queued frame for this slot.
        uint64_t mFrameNumber;
    };

    // mSlots is the array of buffer slots that must be mirrored on the client
    // side. This allows buffer ownership to be transferred between the client
    // and server without sending a GraphicBuffer over binder. The entire array
    // is initialized to NULL at construction time, and buffers are allocated
    // for a slot when requestBuffer is called with that slot's index.
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

    // mDequeueCondition condition used for dequeueBuffer in synchronous mode
    mutable Condition mDequeueCondition;

    // mAbandoned indicates that the GonkNativeWindow will no longer be used to
    // consume buffers pushed to it.
    // It is initialized to false, and set to true in the abandon method.  A
    // GonkNativeWindow that has been abandoned will return the NO_INIT error
    // from all control methods capable of returning an error.
    bool mAbandoned;

    // mTimestamp is the timestamp that will be used for the next buffer queue
    // operation. It defaults to NATIVE_WINDOW_TIMESTAMP_AUTO, which means that
    // a timestamp is auto-generated when queueBuffer is called.
    int64_t mTimestamp;

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultHeight;

    // mPixelFormat holds the pixel format of allocated buffers. It is used
    // in requestBuffers() if a format of zero is specified.
    uint32_t mPixelFormat;

    // mBufferCount is the number of buffer slots that the client and server
    // must maintain. It defaults to MIN_BUFFER_SLOTS + 1 and can be changed
    // by calling setBufferCount or setBufferCountServer
    int mBufferCount;

    // mConnectedApi indicates the API that is currently connected to this
    // GonkNativeWindow.  It defaults to NO_CONNECTED_API (= 0), and gets updated
    // by the connect and disconnect methods.
    int mConnectedApi;

    // mQueue is a FIFO of queued buffers used in synchronous mode
    // GonkNativeWindow always works in synchronous mode
    typedef Vector<int> Fifo;
    Fifo mQueue;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of GonkNativeWindow objects. It must be locked whenever the
    // member variables are accessed.
    mutable Mutex mMutex;

    // mFrameCounter is the free running counter, incremented for every buffer queued
    uint64_t mFrameCounter;

    // mGeneration is the current generation of buffer slots
    uint32_t mGeneration;

    GonkNativeWindowNewFrameCallback* mNewFrameCallback;
};


// CameraGraphicBuffer maintains the buffer returned from GonkNativeWindow
class CameraGraphicBuffer : public mozilla::layers::GraphicBufferLocked
{
    typedef mozilla::layers::SurfaceDescriptor SurfaceDescriptor;
    typedef mozilla::layers::ImageBridgeChild ImageBridgeChild;

public:
    CameraGraphicBuffer(GonkNativeWindow* aNativeWindow,
                        uint32_t aIndex,
                        uint32_t aGeneration,
                        SurfaceDescriptor aBuffer)
        : GraphicBufferLocked(aBuffer)
        , mNativeWindow(aNativeWindow)
        , mIndex(aIndex)
        , mGeneration(aGeneration)
        , mLocked(true)
    {
        DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
    }

    virtual ~CameraGraphicBuffer()
    {
        DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
    }

    // Unlock either returns the buffer to the native window or
    // destroys the buffer if the window is already released.
    virtual void Unlock() MOZ_OVERRIDE
    {
        if (mLocked) {
            // The window might have been destroyed. The buffer is no longer
            // valid at that point.
            sp<GonkNativeWindow> window = mNativeWindow.promote();
            if (window.get() && window->returnBuffer(mIndex, mGeneration)) {
                mLocked = false;
            } else {
                // If the window doesn't exist any more, release the buffer
                // directly.
                ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();
                ibc->DeallocSurfaceDescriptorGralloc(mSurfaceDescriptor);
            }
        }
    }

protected:
    wp<GonkNativeWindow> mNativeWindow;
    uint32_t mIndex;
    uint32_t mGeneration;
    bool mLocked;
};

}; // namespace android

#endif // DOM_CAMERA_GONKNATIVEWINDOW_H
