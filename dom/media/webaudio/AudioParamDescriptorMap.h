/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioParamDescriptor_h
#define mozilla_dom_AudioParamDescriptor_h

#include "mozilla/dom/AudioParamDescriptorBinding.h"
#include "nsTArray.h"

namespace mozilla::dom {

// Note: we call this "map" to match the spec, but we store audio param
// descriptors in an array, because we need ordered access, and don't need the
// map functionalities.
typedef nsTArray<AudioParamDescriptor> AudioParamDescriptorMap;

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioParamDescriptor_h
