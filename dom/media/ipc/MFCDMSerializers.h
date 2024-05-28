/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFCDMSERIALIZERS_H_
#define DOM_MEDIA_IPC_MFCDMSERIALIZERS_H_

#include "MediaData.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/dom/BindingIPCUtils.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::KeySystemConfig::Requirement>
    : public ContiguousEnumSerializerInclusive<
          mozilla::KeySystemConfig::Requirement,
          mozilla::KeySystemConfig::Requirement::Required,
          mozilla::KeySystemConfig::Requirement::NotAllowed> {};

template <>
struct ParamTraits<mozilla::KeySystemConfig::SessionType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::KeySystemConfig::SessionType,
          mozilla::KeySystemConfig::SessionType::Temporary,
          mozilla::KeySystemConfig::SessionType::PersistentLicense> {};

template <>
struct ParamTraits<mozilla::CryptoScheme>
    : public ContiguousEnumSerializerInclusive<
          mozilla::CryptoScheme, mozilla::CryptoScheme::None,
          mozilla::CryptoScheme::Cbcs_1_9> {};

template <>
struct ParamTraits<mozilla::dom::MediaKeyMessageType>
    : public mozilla::dom::WebIDLEnumSerializer<
          mozilla::dom::MediaKeyMessageType> {};

template <>
struct ParamTraits<mozilla::dom::MediaKeyStatus>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::MediaKeyStatus> {
};

template <>
struct ParamTraits<mozilla::dom::HDCPVersion>
    : public mozilla::dom::WebIDLEnumSerializer<mozilla::dom::HDCPVersion> {};

}  // namespace IPC

#endif  // DOM_MEDIA_IPC_MFCDMSERIALIZERS_H_
