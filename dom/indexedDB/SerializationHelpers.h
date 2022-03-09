/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_serializationhelpers_h__
#define mozilla_dom_indexeddb_serializationhelpers_h__

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"

#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/indexedDB/KeyPath.h"
#include "mozilla/dom/IDBCursor.h"
#include "mozilla/dom/IDBTransaction.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::indexedDB::StructuredCloneFileBase::FileType>
    : public ContiguousEnumSerializer<
          mozilla::dom::indexedDB::StructuredCloneFileBase::FileType,
          mozilla::dom::indexedDB::StructuredCloneFileBase::eBlob,
          mozilla::dom::indexedDB::StructuredCloneFileBase::eEndGuard> {};

template <>
struct ParamTraits<mozilla::dom::indexedDB::Key> {
  typedef mozilla::dom::indexedDB::Key paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mBuffer);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mBuffer);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.mBuffer, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath::KeyPathType>
    : public ContiguousEnumSerializer<
          mozilla::dom::indexedDB::KeyPath::KeyPathType,
          mozilla::dom::indexedDB::KeyPath::KeyPathType::NonExistent,
          mozilla::dom::indexedDB::KeyPath::KeyPathType::EndGuard> {};

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath> {
  typedef mozilla::dom::indexedDB::KeyPath paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mStrings);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mType) &&
           ReadParam(aReader, &aResult->mStrings);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.mStrings, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::IDBCursor::Direction>
    : public ContiguousEnumSerializer<
          mozilla::dom::IDBCursor::Direction,
          mozilla::dom::IDBCursor::Direction::Next,
          mozilla::dom::IDBCursor::Direction::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::IDBTransaction::Mode>
    : public ContiguousEnumSerializer<
          mozilla::dom::IDBTransaction::Mode,
          mozilla::dom::IDBTransaction::Mode::ReadOnly,
          mozilla::dom::IDBTransaction::Mode::Invalid> {};

}  // namespace IPC

#endif  // mozilla_dom_indexeddb_serializationhelpers_h__
