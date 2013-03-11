/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef GONK_CAMERA_LISTENER_H
#define GONK_CAMERA_LISTENER_H

#include <utils/Timers.h>
#include <camera/Camera.h>

namespace android {

// ref-counted object for callbacks
class GonkCameraListener: virtual public RefBase
{
public:
    virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2) = 0;
    virtual void postData(int32_t msgType, const sp<IMemory>& dataPtr,
                          camera_frame_metadata_t *metadata) = 0;
    virtual void postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) = 0;
};

}; // namespace android

#endif
