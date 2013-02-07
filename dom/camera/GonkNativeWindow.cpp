/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
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

#include "base/basictypes.h"
#include "GonkCameraHwMgr.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "GonkNativeWindow.h"
#include "nsDebug.h"

/**
 * DOM_CAMERA_LOGI() is enabled in debug builds, and turned on by setting
 * NSPR_LOG_MODULES=Camera:N environment variable, where N >= 3.
 *
 * CNW_LOGE() is always enabled.
 */
#define CNW_LOGD(...)   DOM_CAMERA_LOGI(__VA_ARGS__)
#define CNW_LOGE(...)   {(void)printf_stderr(__VA_ARGS__);}

using namespace android;
using namespace mozilla::layers;

GonkNativeWindow::GonkNativeWindow(GonkNativeWindowNewFrameCallback* aCallback)
  : mNewFrameCallback(aCallback)
{
    GonkNativeWindow::init();
}

GonkNativeWindow::GonkNativeWindow()
  : mNewFrameCallback(nullptr)
{
    GonkNativeWindow::init();
}

GonkNativeWindow::~GonkNativeWindow()
{
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    freeAllBuffersLocked(freeList);
    releaseBufferFreeListUnlocked(freeList);
}

void GonkNativeWindow::abandon()
{
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;
    {
        Mutex::Autolock lock(mMutex);
        mAbandoned = true;
        freeAllBuffersLocked(freeList);
    }

    releaseBufferFreeListUnlocked(freeList);
    mDequeueCondition.signal();
}

void GonkNativeWindow::init()
{
    // Initialize the ANativeWindow function pointers.
    ANativeWindow::setSwapInterval  = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer    = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer     = hook_cancelBuffer;
    ANativeWindow::lockBuffer       = hook_lockBuffer;
    ANativeWindow::queueBuffer      = hook_queueBuffer;
    ANativeWindow::query            = hook_query;
    ANativeWindow::perform          = hook_perform;

    mDefaultWidth = 0;
    mDefaultHeight = 0;
    mPixelFormat = 0;
    mUsage = 0;
    mTimestamp = NATIVE_WINDOW_TIMESTAMP_AUTO;
    mBufferCount = MIN_BUFFER_SLOTS;
    mFrameCounter = 0;
    mGeneration = 0;
    mAbandoned =false;
}


int GonkNativeWindow::hook_setSwapInterval(ANativeWindow* window, int interval)
{
    GonkNativeWindow* c = getSelf(window);
    return c->setSwapInterval(interval);
}

int GonkNativeWindow::hook_dequeueBuffer(ANativeWindow* window,
        ANativeWindowBuffer** buffer)
{
    GonkNativeWindow* c = getSelf(window);
    return c->dequeueBuffer(buffer);
}

int GonkNativeWindow::hook_cancelBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer)
{
    GonkNativeWindow* c = getSelf(window);
    return c->cancelBuffer(buffer);
}

int GonkNativeWindow::hook_lockBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer)
{
    GonkNativeWindow* c = getSelf(window);
    return c->lockBuffer(buffer);
}

int GonkNativeWindow::hook_queueBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer)
{
    GonkNativeWindow* c = getSelf(window);
    return c->queueBuffer(buffer);
}

int GonkNativeWindow::hook_query(const ANativeWindow* window,
                                int what, int* value)
{
    const GonkNativeWindow* c = getSelf(window);
    return c->query(what, value);
}

int GonkNativeWindow::hook_perform(ANativeWindow* window, int operation, ...)
{
    va_list args;
    va_start(args, operation);
    GonkNativeWindow* c = getSelf(window);
    return c->perform(operation, args);
}

void GonkNativeWindow::freeAllBuffersLocked(nsTArray<SurfaceDescriptor>& freeList)
{
    CNW_LOGD("freeAllBuffersLocked: from generation %d", mGeneration);
    ++mGeneration;

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            // Don't try to destroy the gralloc buffer if it is still in the
            // video stream awaiting rendering.
            if (mSlots[i].mBufferState != BufferSlot::RENDERING) {
                SurfaceDescriptor* desc = freeList.AppendElement();
                *desc = mSlots[i].mSurfaceDescriptor;
            }
            mSlots[i].mGraphicBuffer = NULL;
            mSlots[i].mBufferState = BufferSlot::FREE;
            mSlots[i].mFrameNumber = 0;
        }
    }
}

void GonkNativeWindow::releaseBufferFreeListUnlocked(nsTArray<SurfaceDescriptor>& freeList)
{
    // This function MUST ONLY be called with mMutex unlocked; else there
    // is a risk of deadlock with the ImageBridge thread.

    CNW_LOGD("releaseBufferFreeListUnlocked: E");
    ImageBridgeChild *ibc = ImageBridgeChild::GetSingleton();

    for (uint32_t i = 0; i < freeList.Length(); ++i) {
        ibc->DeallocSurfaceDescriptorGralloc(freeList[i]);
    }

    freeList.Clear();
    CNW_LOGD("releaseBufferFreeListUnlocked: X");
}

void GonkNativeWindow::clearRenderingStateBuffersLocked()
{
    ++mGeneration;

    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mGraphicBuffer != NULL) {
            // Don't try to destroy the gralloc buffer if it is still in the
            // video stream awaiting rendering.
            if (mSlots[i].mBufferState == BufferSlot::RENDERING) {
                mSlots[i].mGraphicBuffer = NULL;
                mSlots[i].mBufferState = BufferSlot::FREE;
                mSlots[i].mFrameNumber = 0;
            }
        }
    }
}

int GonkNativeWindow::setBufferCount(int bufferCount)
{
    CNW_LOGD("setBufferCount: count=%d", bufferCount);
    nsAutoTArray<SurfaceDescriptor, NUM_BUFFER_SLOTS> freeList;

    {
        Mutex::Autolock lock(mMutex);

        if (mAbandoned) {
            CNW_LOGE("setBufferCount: GonkNativeWindow has been abandoned!");
            return NO_INIT;
        }

        if (bufferCount > NUM_BUFFER_SLOTS) {
            CNW_LOGE("setBufferCount: bufferCount larger than slots available");
            return BAD_VALUE;
        }

        if (bufferCount < MIN_BUFFER_SLOTS) {
            CNW_LOGE("setBufferCount: requested buffer count (%d) is less than "
                    "minimum (%d)", bufferCount, MIN_BUFFER_SLOTS);
            return BAD_VALUE;
        }

        // Error out if the user has dequeued buffers.
        for (int i=0 ; i<mBufferCount ; i++) {
            if (mSlots[i].mBufferState == BufferSlot::DEQUEUED) {
                CNW_LOGE("setBufferCount: client owns some buffers");
                return -EINVAL;
            }
        }

        if (bufferCount >= mBufferCount) {
            mBufferCount = bufferCount;
            //clear only buffers in RENDERING state.
            clearRenderingStateBuffersLocked();
            mDequeueCondition.signal();
            return OK;
        }

        // reducing the number of buffers
        // here we're guaranteed that the client doesn't have dequeued buffers
        // and will release all of its buffer references.
        freeAllBuffersLocked(freeList);
        mBufferCount = bufferCount;
        mDequeueCondition.signal();
    }

    releaseBufferFreeListUnlocked(freeList);
    return OK;
}

int GonkNativeWindow::dequeueBuffer(android_native_buffer_t** buffer)
{
    uint32_t defaultWidth;
    uint32_t defaultHeight;
    uint32_t pixelFormat;
    uint32_t usage;
    uint32_t generation;
    bool alloc = false;
    int buf = INVALID_BUFFER_SLOT;
    SurfaceDescriptor descOld;

    {
        Mutex::Autolock lock(mMutex);
        generation = mGeneration;

        int found = -1;
        int dequeuedCount = 0;
        bool tryAgain = true;

        CNW_LOGD("dequeueBuffer: E");
        while (tryAgain) {
            if (mAbandoned) {
                CNW_LOGE("dequeueBuffer: GonkNativeWindow has been abandoned!");
                return NO_INIT;
            }
            // look for a free buffer to give to the client
            found = INVALID_BUFFER_SLOT;
            dequeuedCount = 0;
            for (int i = 0; i < mBufferCount; i++) {
                const int state = mSlots[i].mBufferState;
                if (state == BufferSlot::DEQUEUED) {
                    dequeuedCount++;
                }
                else if (state == BufferSlot::FREE) {
                    /* We return the oldest of the free buffers to avoid
                     * stalling the producer if possible.  This is because
                     * the consumer may still have pending reads of the
                     * buffers in flight.
                     */
                    if (found < 0 ||
                        mSlots[i].mFrameNumber < mSlots[found].mFrameNumber) {
                        found = i;
                    }
                }
            }

            // we're in synchronous mode and didn't find a buffer, we need to
            // wait for some buffers to be consumed
            tryAgain = (found == INVALID_BUFFER_SLOT);
            if (tryAgain) {
                CNW_LOGD("dequeueBuffer: Try again");
                mDequeueCondition.wait(mMutex);
                CNW_LOGD("dequeueBuffer: Now");
            }
        }

        if (found == INVALID_BUFFER_SLOT) {
            // This should not happen.
            CNW_LOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        buf = found;

        // buffer is now in DEQUEUED
        mSlots[buf].mBufferState = BufferSlot::DEQUEUED;

        const sp<GraphicBuffer>& gbuf(mSlots[buf].mGraphicBuffer);
        alloc = (gbuf == NULL);
        if ((gbuf!=NULL) &&
           ((uint32_t(gbuf->width)  != mDefaultWidth) ||
            (uint32_t(gbuf->height) != mDefaultHeight) ||
            (uint32_t(gbuf->format) != mPixelFormat) ||
            ((uint32_t(gbuf->usage) & mUsage) != mUsage))) {
            alloc = true;
            descOld = mSlots[buf].mSurfaceDescriptor;
        }

        if (alloc) {
            // get local copies for graphics buffer allocations
            defaultWidth = mDefaultWidth;
            defaultHeight = mDefaultHeight;
            pixelFormat = mPixelFormat;
            usage = mUsage;
        }
    }
    
    // At this point, the buffer is now marked DEQUEUED, and no one else
    // should touch it, except for freeAllBuffersLocked(); we handle that
    // after trying to create the surface descriptor below.
    //
    // So we don't need mMutex locked, which would otherwise run the risk
    // of a deadlock on calling AllocSurfaceDescriptorGralloc().    

    SurfaceDescriptor desc;
    ImageBridgeChild* ibc;
    sp<GraphicBuffer> graphicBuffer;
    if (alloc) {
        status_t error;
        ibc = ImageBridgeChild::GetSingleton();
        CNW_LOGD("dequeueBuffer: about to alloc surface descriptor");
        ibc->AllocSurfaceDescriptorGralloc(gfxIntSize(defaultWidth, defaultHeight),
                                           pixelFormat,
                                           usage,
                                           &desc);
        // We can only use a gralloc buffer here.  If we didn't get
        // one back, something went wrong.
        CNW_LOGD("dequeueBuffer: got surface descriptor");
        if (SurfaceDescriptor::TSurfaceDescriptorGralloc != desc.type()) {
            MOZ_ASSERT(SurfaceDescriptor::T__None == desc.type());
            CNW_LOGE("dequeueBuffer: failed to alloc gralloc buffer");
            return -ENOMEM;
        }
        graphicBuffer = GrallocBufferActor::GetFrom(desc.get_SurfaceDescriptorGralloc());
        error = graphicBuffer->initCheck();
        if (error != NO_ERROR) {
            CNW_LOGE("dequeueBuffer: createGraphicBuffer failed with error %d", error);
            return error;
        }
    }

    bool tooOld = false;
    {
        Mutex::Autolock lock(mMutex);
        if (generation == mGeneration) {
            if (alloc) {
                mSlots[buf].mGraphicBuffer = graphicBuffer;
                mSlots[buf].mSurfaceDescriptor = desc;
                mSlots[buf].mSurfaceDescriptor.get_SurfaceDescriptorGralloc().external() = true;
            }
            *buffer = mSlots[buf].mGraphicBuffer.get();

            CNW_LOGD("dequeueBuffer: returning slot=%d buf=%p ", buf,
                    mSlots[buf].mGraphicBuffer->handle);
        } else {
            *buffer = nullptr;
            tooOld = true;
        }
    }

    if (alloc && IsSurfaceDescriptorValid(descOld)) {
        ibc->DeallocSurfaceDescriptorGralloc(descOld);
    }

    if (alloc && tooOld) {
        ibc->DeallocSurfaceDescriptorGralloc(desc);
    }

    CNW_LOGD("dequeueBuffer: X");
    return NO_ERROR;
}

int GonkNativeWindow::getSlotFromBufferLocked(
        android_native_buffer_t* buffer) const
{
    if (buffer == NULL) {
        CNW_LOGE("getSlotFromBufferLocked: encountered NULL buffer");
        return BAD_VALUE;
    }

    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].mGraphicBuffer != NULL && mSlots[i].mGraphicBuffer->handle == buffer->handle) {
            return i;
        }
    }
    CNW_LOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return BAD_VALUE;
}

mozilla::layers::SurfaceDescriptor *
GonkNativeWindow::getSurfaceDescriptorFromBuffer(ANativeWindowBuffer* buffer)
{
  int buf = getSlotFromBufferLocked(buffer);
  if (buf < 0 || buf >= mBufferCount ||
      mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
    return nullptr;
  }

  return &mSlots[buf].mSurfaceDescriptor;
}

int GonkNativeWindow::queueBuffer(ANativeWindowBuffer* buffer)
{
    {
        Mutex::Autolock lock(mMutex);
        CNW_LOGD("queueBuffer: E");

        if (mAbandoned) {
            CNW_LOGE("queueBuffer: GonkNativeWindow has been abandoned!");
            return NO_INIT;
        }

        int buf = getSlotFromBufferLocked(buffer);

        if (buf < 0 || buf >= mBufferCount) {
            CNW_LOGE("queueBuffer: slot index out of range [0, %d]: %d",
                     mBufferCount, buf);
            return -EINVAL;
        } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
            CNW_LOGE("queueBuffer: slot %d is not owned by the client "
                     "(state=%d)", buf, mSlots[buf].mBufferState);
            return -EINVAL;
        }

        int64_t timestamp;
        if (mTimestamp == NATIVE_WINDOW_TIMESTAMP_AUTO) {
            timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        } else {
            timestamp = mTimestamp;
        }

        mSlots[buf].mBufferState = BufferSlot::QUEUED;
        mSlots[buf].mTimestamp = timestamp;
        mFrameCounter++;
        mSlots[buf].mFrameNumber = mFrameCounter;

        mDequeueCondition.signal();
    }

    // OnNewFrame might call lockCurrentBuffer so we must release the
    // mutex first.
    if (mNewFrameCallback) {
      mNewFrameCallback->OnNewFrame();
    }
    CNW_LOGD("queueBuffer: X");
    return OK;
}


already_AddRefed<GraphicBufferLocked>
GonkNativeWindow::getCurrentBuffer()
{
  CNW_LOGD("GonkNativeWindow::getCurrentBuffer");
  Mutex::Autolock lock(mMutex);

  if (mAbandoned) {
    CNW_LOGE("getCurrentBuffer: GonkNativeWindow has been abandoned!");
    return NULL;
  }

  int found = -1;
  for (int i = 0; i < mBufferCount; i++) {
    const int state = mSlots[i].mBufferState;
    if (state == BufferSlot::QUEUED) {
      if (found < 0 ||
          mSlots[i].mFrameNumber < mSlots[found].mFrameNumber) {
        found = i;
      }
    }
  }

  if (found < 0) {
    mDequeueCondition.signal();
    return NULL;
  }

  mSlots[found].mBufferState = BufferSlot::RENDERING;

  nsRefPtr<GraphicBufferLocked> ret =
    new CameraGraphicBuffer(this, found, mGeneration, mSlots[found].mSurfaceDescriptor);
  mDequeueCondition.signal();
  return ret.forget();
}

bool
GonkNativeWindow::returnBuffer(uint32_t aIndex, uint32_t aGeneration)
{
  CNW_LOGD("GonkNativeWindow::returnBuffer: slot=%d (generation=%d)", aIndex, aGeneration);
  Mutex::Autolock lock(mMutex);

  if (mAbandoned) {
    CNW_LOGD("returnBuffer: GonkNativeWindow has been abandoned!");
    return false;
  }

  if (aGeneration != mGeneration) {
    CNW_LOGD("returnBuffer: buffer is from generation %d (current is %d)",
      aGeneration, mGeneration);
    return false;
  }
  if (aIndex >= mBufferCount) {
    CNW_LOGE("returnBuffer: slot index out of range [0, %d]: %d",
             mBufferCount, aIndex);
    return false;
  }
  if (mSlots[aIndex].mBufferState != BufferSlot::RENDERING) {
    CNW_LOGE("returnBuffer: slot %d is not owned by the compositor (state=%d)",
                  aIndex, mSlots[aIndex].mBufferState);
    return false;
  }

  mSlots[aIndex].mBufferState = BufferSlot::FREE;
  mDequeueCondition.signal();
  return true;
}

int GonkNativeWindow::lockBuffer(ANativeWindowBuffer* buffer)
{
    CNW_LOGD("GonkNativeWindow::lockBuffer");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("lockBuffer: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }
    return OK;
}

int GonkNativeWindow::cancelBuffer(ANativeWindowBuffer* buffer)
{
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGD("cancelBuffer: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }
    int buf = getSlotFromBufferLocked(buffer);

    CNW_LOGD("cancelBuffer: slot=%d", buf);
    if (buf < 0 || buf >= mBufferCount) {
        CNW_LOGE("cancelBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, buf);
        return -EINVAL;
    } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
        CNW_LOGE("cancelBuffer: slot %d is not owned by the client (state=%d)",
                buf, mSlots[buf].mBufferState);
        return -EINVAL;
    }
    mSlots[buf].mBufferState = BufferSlot::FREE;
    mSlots[buf].mFrameNumber = 0;
    mDequeueCondition.signal();
    return OK;
}

int GonkNativeWindow::perform(int operation, va_list args)
{
    switch (operation) {
        case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
        case NATIVE_WINDOW_SET_BUFFERS_SIZE:
        case NATIVE_WINDOW_SET_SCALING_MODE:
        case NATIVE_WINDOW_SET_CROP:
        case NATIVE_WINDOW_CONNECT:
        case NATIVE_WINDOW_DISCONNECT:
            // deprecated. must return NO_ERROR.
            return NO_ERROR;
        case NATIVE_WINDOW_SET_USAGE:
            return dispatchSetUsage(args);
        case NATIVE_WINDOW_SET_BUFFER_COUNT:
            return dispatchSetBufferCount(args);
        case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
            return dispatchSetBuffersGeometry(args);
        case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
            return dispatchSetBuffersTimestamp(args);
        case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
            return dispatchSetBuffersDimensions(args);
        case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
            return dispatchSetBuffersFormat(args);
        case NATIVE_WINDOW_LOCK:
        case NATIVE_WINDOW_UNLOCK_AND_POST:
        case NATIVE_WINDOW_API_CONNECT:
        case NATIVE_WINDOW_API_DISCONNECT:
        default:
            NS_WARNING("Unsupported operation");
            return INVALID_OPERATION;
    }
}

int GonkNativeWindow::query(int what, int* outValue) const
{
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("query: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    int value;
    switch (what) {
    case NATIVE_WINDOW_WIDTH:
        value = mDefaultWidth;
        break;
    case NATIVE_WINDOW_HEIGHT:
        value = mDefaultHeight;
        break;
    case NATIVE_WINDOW_FORMAT:
        value = mPixelFormat;
        break;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        value = MIN_UNDEQUEUED_BUFFERS;
        break;
    default:
        return BAD_VALUE;
    }
    outValue[0] = value;
    return NO_ERROR;
}

int GonkNativeWindow::setSwapInterval(int interval)
{
    return NO_ERROR;
}

int GonkNativeWindow::dispatchSetUsage(va_list args)
{
    int usage = va_arg(args, int);
    return setUsage(usage);
}

int GonkNativeWindow::dispatchSetBufferCount(va_list args)
{
    size_t bufferCount = va_arg(args, size_t);
    return setBufferCount(bufferCount);
}

int GonkNativeWindow::dispatchSetBuffersGeometry(va_list args)
{
    int w = va_arg(args, int);
    int h = va_arg(args, int);
    int f = va_arg(args, int);
    int err = setBuffersDimensions(w, h);
    if (err != 0) {
        return err;
    }
    return setBuffersFormat(f);
}

int GonkNativeWindow::dispatchSetBuffersDimensions(va_list args)
{
    int w = va_arg(args, int);
    int h = va_arg(args, int);
    return setBuffersDimensions(w, h);
}

int GonkNativeWindow::dispatchSetBuffersFormat(va_list args)
{
    int f = va_arg(args, int);
    return setBuffersFormat(f);
}

int GonkNativeWindow::dispatchSetBuffersTimestamp(va_list args)
{
    int64_t timestamp = va_arg(args, int64_t);
    return setBuffersTimestamp(timestamp);
}

int GonkNativeWindow::setUsage(uint32_t reqUsage)
{
    CNW_LOGD("GonkNativeWindow::setUsage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("setUsage: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }
    mUsage = reqUsage;
    return OK;
}

int GonkNativeWindow::setBuffersDimensions(int w, int h)
{
    CNW_LOGD("GonkNativeWindow::setBuffersDimensions");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("setBuffersDimensions: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    if (w<0 || h<0)
        return BAD_VALUE;

    if ((w && !h) || (!w && h))
        return BAD_VALUE;

    mDefaultWidth = w;
    mDefaultHeight = h;

    return OK;
}

int GonkNativeWindow::setBuffersFormat(int format)
{
    CNW_LOGD("GonkNativeWindow::setBuffersFormat");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("setBuffersFormat: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    if (format<0)
        return BAD_VALUE;

    mPixelFormat = format;

    return NO_ERROR;
}

int GonkNativeWindow::setBuffersTimestamp(int64_t timestamp)
{
    CNW_LOGD("GonkNativeWindow::setBuffersTimestamp");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        CNW_LOGE("setBuffersTimestamp: GonkNativeWindow has been abandoned!");
        return NO_INIT;
    }

    mTimestamp = timestamp;
    return OK;
}
