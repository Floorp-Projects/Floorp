/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GVAUTOPLAYREQUESTSTATUSIPC_H_
#define DOM_MEDIA_GVAUTOPLAYREQUESTSTATUSIPC_H_

#include "ipc/EnumSerializer.h"

#include "GVAutoplayRequestUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::GVAutoplayRequestStatus>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::GVAutoplayRequestStatus,
          mozilla::dom::GVAutoplayRequestStatus::eUNKNOWN,
          mozilla::dom::GVAutoplayRequestStatus::ePENDING> {};

}  // namespace IPC

#endif  // DOM_MEDIA_GVAUTOPLAYREQUESTSTATUSIPC_H_
