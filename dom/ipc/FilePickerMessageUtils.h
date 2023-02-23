/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_filepicker_message_utils_h__
#define mozilla_dom_filepicker_message_utils_h__

#include "ipc/EnumSerializer.h"
#include "nsIFilePicker.h"

namespace IPC {
template <>
struct ParamTraits<nsIFilePicker::Mode>
    : public ContiguousEnumSerializerInclusive<
          nsIFilePicker::Mode, nsIFilePicker::Mode::modeOpen,
          nsIFilePicker::Mode::modeOpenMultiple> {};

template <>
struct ParamTraits<nsIFilePicker::CaptureTarget>
    : public ContiguousEnumSerializerInclusive<
          nsIFilePicker::CaptureTarget,
          nsIFilePicker::CaptureTarget::captureNone,
          nsIFilePicker::CaptureTarget::captureEnv> {};

template <>
struct ParamTraits<nsIFilePicker::ResultCode>
    : public ContiguousEnumSerializerInclusive<
          nsIFilePicker::ResultCode, nsIFilePicker::ResultCode::returnOK,
          nsIFilePicker::ResultCode::returnReplace> {};
}  // namespace IPC

#endif  // mozilla_dom_filepicker_message_utils_h__
