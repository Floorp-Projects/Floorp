/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_IPCTypes_h
#define mozilla_a11y_IPCTypes_h

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/AccAttributes.h"
#  include "mozilla/a11y/AccTypes.h"
#  include "mozilla/a11y/CacheConstants.h"
#  include "mozilla/a11y/Role.h"
#  include "mozilla/a11y/AccGroupInfo.h"
#  include "mozilla/GfxMessageUtils.h"
#  include "ipc/EnumSerializer.h"
#  include "ipc/IPCMessageUtilsSpecializations.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::a11y::role>
    : public ContiguousEnumSerializerInclusive<mozilla::a11y::role,
                                               mozilla::a11y::role::NOTHING,
                                               mozilla::a11y::role::LAST_ROLE> {
};

template <>
struct ParamTraits<mozilla::a11y::AccType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::a11y::AccType, mozilla::a11y::AccType::eNoType,
          mozilla::a11y::AccType::eLastAccType> {};

template <>
struct ParamTraits<mozilla::a11y::AccGenericType>
    : public BitFlagsEnumSerializer<
          mozilla::a11y::AccGenericType,
          mozilla::a11y::AccGenericType::eAllGenericTypes> {};

template <>
struct ParamTraits<mozilla::a11y::CacheUpdateType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::a11y::CacheUpdateType,
          mozilla::a11y::CacheUpdateType::Initial,
          mozilla::a11y::CacheUpdateType::Update> {};

template <>
struct ParamTraits<mozilla::a11y::FontSize> {
  typedef mozilla::a11y::FontSize paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::a11y::DeleteEntry> {
  typedef mozilla::a11y::DeleteEntry paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::a11y::AccGroupInfo> {
  typedef mozilla::a11y::AccGroupInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    MOZ_ASSERT_UNREACHABLE("Cannot serialize AccGroupInfo");
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    MOZ_ASSERT_UNREACHABLE("Cannot de-serialize AccGroupInfo");
    return false;
  }
};

template <>
struct ParamTraits<mozilla::a11y::Color> {
  typedef mozilla::a11y::Color paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::a11y::AccAttributes*> {
  typedef mozilla::a11y::AccAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType* aParam) {
    if (!aParam) {
      WriteParam(aWriter, true);
      return;
    }

    WriteParam(aWriter, false);
    uint32_t count = aParam->mData.Count();
    WriteParam(aWriter, count);
    for (auto iter = aParam->mData.ConstIter(); !iter.Done(); iter.Next()) {
      RefPtr<nsAtom> key = iter.Key();
      WriteParam(aWriter, key);
      const paramType::AttrValueType& data = iter.Data();
      WriteParam(aWriter, data);
    }
  }

  static bool Read(MessageReader* aReader, RefPtr<paramType>* aResult) {
    bool isNull = false;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    *aResult = mozilla::MakeRefPtr<mozilla::a11y::AccAttributes>();
    uint32_t count;
    if (!ReadParam(aReader, &count)) {
      return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
      RefPtr<nsAtom> key;
      if (!ReadParam(aReader, &key)) {
        return false;
      }
      paramType::AttrValueType val(0);
      if (!ReadParam(aReader, &val)) {
        return false;
      }
      (*aResult)->mData.InsertOrUpdate(key, std::move(val));
    }
    return true;
  }
};

}  // namespace IPC
#else
namespace mozilla {
namespace a11y {
typedef uint32_t role;
}  // namespace a11y
}  // namespace mozilla
#endif  // ACCESSIBILITY

/**
 * Since IPDL does not support preprocessing, this header file allows us to
 * define types used by PDocAccessible differently depending on platform.
 */

#if defined(MOZ_WIDGET_COCOA)
#  if defined(ACCESSIBILITY)
#    include "mozilla/a11y/PlatformExtTypes.h"
namespace IPC {

template <>
struct ParamTraits<mozilla::a11y::EWhichRange>
    : public ContiguousEnumSerializerInclusive<
          mozilla::a11y::EWhichRange, mozilla::a11y::EWhichRange::eLeftWord,
          mozilla::a11y::EWhichRange::eStyle> {};

}  // namespace IPC

#  else
namespace mozilla {
namespace a11y {
typedef uint32_t EWhichRange;
}  // namespace a11y
}  // namespace mozilla
#  endif  // defined(ACCESSIBILITY)
#endif    // defined(MOZ_WIDGET_COCOA)

#endif  // mozilla_a11y_IPCTypes_h
