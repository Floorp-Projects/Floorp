/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaSystemResourceMessageUtils_h_)
#  define MediaSystemResourceMessageUtils_h_

#  include "ipc/EnumSerializer.h"
#  include "MediaSystemResourceTypes.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::MediaSystemResourceType>
    : public ContiguousEnumSerializer<
          mozilla::MediaSystemResourceType,
          mozilla::MediaSystemResourceType::VIDEO_DECODER,
          mozilla::MediaSystemResourceType::INVALID_RESOURCE> {};

}  // namespace IPC

#endif
