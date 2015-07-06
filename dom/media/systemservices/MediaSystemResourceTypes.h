/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceTypes_h_)
#define MediaSystemResourceTypes_h_

namespace mozilla {

enum class MediaSystemResourceType : uint32_t {
  VIDEO_DECODER = 0,
  AUDIO_DECODER,  // Not supported currently.
  VIDEO_ENCODER,
  AUDIO_ENCODER,  // Not supported currently.
  CAMERA,          // Not supported currently.
  INVALID_RESOURCE,
};

} // namespace mozilla

#endif
