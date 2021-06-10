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
#  include "mozilla/a11y/Role.h"
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
struct ParamTraits<mozilla::a11y::FontSize> {
  typedef mozilla::a11y::FontSize paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::a11y::Color> {
  typedef mozilla::a11y::Color paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mValue));
  }
};

template <>
struct ParamTraits<mozilla::a11y::AccAttributes*> {
  typedef mozilla::a11y::AccAttributes paramType;

  static void Write(Message* aMsg, const paramType* aParam) {
    if (!aParam) {
      WriteParam(aMsg, true);
      return;
    }

    WriteParam(aMsg, false);
    uint32_t count = aParam->mData.Count();
    WriteParam(aMsg, count);
    for (auto iter = aParam->mData.ConstIter(); !iter.Done(); iter.Next()) {
      RefPtr<nsAtom> key = iter.Key();
      WriteParam(aMsg, key);
      const paramType::AttrValueType& data = iter.Data();
      WriteParam(aMsg, data);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   RefPtr<paramType>* aResult) {
    bool isNull = false;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    *aResult = mozilla::MakeRefPtr<mozilla::a11y::AccAttributes>();
    uint32_t count;
    if (!ReadParam(aMsg, aIter, &count)) {
      return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
      RefPtr<nsAtom> key;
      if (!ReadParam(aMsg, aIter, &key)) {
        return false;
      }
      paramType::AttrValueType val(0);
      if (!ReadParam(aMsg, aIter, &val)) {
        return false;
      }
      (*aResult)->mData.InsertOrUpdate(key, val);
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

#if defined(XP_WIN) && defined(ACCESSIBILITY)

// So that we don't include a bunch of other Windows junk.
#  if !defined(COM_NO_WINDOWS_H)
#    define COM_NO_WINDOWS_H
#  endif  // !defined(COM_NO_WINDOWS_H)

// COM headers pull in MSXML which conflicts with our own XMLDocument class.
// This define excludes those conflicting definitions.
#  if !defined(__XMLDocument_FWD_DEFINED__)
#    define __XMLDocument_FWD_DEFINED__
#  endif  // !defined(__XMLDocument_FWD_DEFINED__)

#  include <combaseapi.h>

#  include "mozilla/a11y/COMPtrTypes.h"

// This define in rpcndr.h messes up our code, so we must undefine it after
// COMPtrTypes.h has been included.
#  if defined(small)
#    undef small
#  endif  // defined(small)

#else

namespace mozilla {
namespace a11y {

typedef uint32_t IAccessibleHolder;
typedef uint32_t IDispatchHolder;
typedef uint32_t IHandlerControlHolder;

}  // namespace a11y
}  // namespace mozilla

#endif  // defined(XP_WIN) && defined(ACCESSIBILITY)

#if defined(MOZ_WIDGET_COCOA)
#  if defined(ACCESSIBILITY)
#    include "mozilla/a11y/PlatformExtTypes.h"
namespace IPC {

template <>
struct ParamTraits<mozilla::a11y::EWhichRange>
    : public ContiguousEnumSerializerInclusive<
          mozilla::a11y::EWhichRange, mozilla::a11y::EWhichRange::eLeftWord,
          mozilla::a11y::EWhichRange::eStyle> {};

template <>
struct ParamTraits<mozilla::a11y::EWhichPostFilter>
    : public ContiguousEnumSerializerInclusive<
          mozilla::a11y::EWhichPostFilter,
          mozilla::a11y::EWhichPostFilter::eContainsText,
          mozilla::a11y::EWhichPostFilter::eContainsText> {};

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
