/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_volumemanagerlog_h__
#define mozilla_system_volumemanagerlog_h__

#define USE_DEBUG 0

#undef LOG
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO,  "VolumeManager" , ## args)
#define ERR(args...)  __android_log_print(ANDROID_LOG_ERROR, "VolumeManager" , ## args)

#if USE_DEBUG
#define DBG(args...)  __android_log_print(ANDROID_LOG_DEBUG, "VolumeManager" , ## args)
#else
#define DBG(args...)
#endif

#endif  // mozilla_system_volumemanagerlog_h__
