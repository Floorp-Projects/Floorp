/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_serializationhelpers_h__
#define mozilla_dom_indexeddb_serializationhelpers_h__

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/indexedDB/DatabaseInfo.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/indexedDB/KeyPath.h"
#include "mozilla/dom/indexedDB/IDBCursor.h"
#include "mozilla/dom/indexedDB/IDBTransaction.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::indexedDB::Key>
{
  typedef mozilla::dom::indexedDB::Key paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBuffer);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mBuffer);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.mBuffer, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath::KeyPathType> :
  public EnumSerializer<mozilla::dom::indexedDB::KeyPath::KeyPathType,
                        mozilla::dom::indexedDB::KeyPath::NONEXISTENT,
                        mozilla::dom::indexedDB::KeyPath::ENDGUARD>
{ };

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath>
{
  typedef mozilla::dom::indexedDB::KeyPath paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mStrings);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mStrings);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.mStrings, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::IDBCursor::Direction> :
  public EnumSerializer<mozilla::dom::indexedDB::IDBCursor::Direction,
                        mozilla::dom::indexedDB::IDBCursor::NEXT,
                        mozilla::dom::indexedDB::IDBCursor::DIRECTION_INVALID>
{ };

template <>
struct ParamTraits<mozilla::dom::indexedDB::IDBTransaction::Mode> :
  public EnumSerializer<mozilla::dom::indexedDB::IDBTransaction::Mode,
                        mozilla::dom::indexedDB::IDBTransaction::READ_ONLY,
                        mozilla::dom::indexedDB::IDBTransaction::MODE_INVALID>
{ };

template <>
struct ParamTraits<mozilla::dom::indexedDB::IndexInfo>
{
  typedef mozilla::dom::indexedDB::IndexInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.name);
    WriteParam(aMsg, aParam.id);
    WriteParam(aMsg, aParam.keyPath);
    WriteParam(aMsg, aParam.unique);
    WriteParam(aMsg, aParam.multiEntry);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->name) &&
           ReadParam(aMsg, aIter, &aResult->id) &&
           ReadParam(aMsg, aIter, &aResult->keyPath) &&
           ReadParam(aMsg, aIter, &aResult->unique) &&
           ReadParam(aMsg, aIter, &aResult->multiEntry);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.name, aLog);
    LogParam(aParam.id, aLog);
    LogParam(aParam.keyPath, aLog);
    LogParam(aParam.unique, aLog);
    LogParam(aParam.multiEntry, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::ObjectStoreInfoGuts>
{
  typedef mozilla::dom::indexedDB::ObjectStoreInfoGuts paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.name);
    WriteParam(aMsg, aParam.id);
    WriteParam(aMsg, aParam.keyPath);
    WriteParam(aMsg, aParam.autoIncrement);
    WriteParam(aMsg, aParam.indexes);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->name) &&
           ReadParam(aMsg, aIter, &aResult->id) &&
           ReadParam(aMsg, aIter, &aResult->keyPath) &&
           ReadParam(aMsg, aIter, &aResult->autoIncrement) &&
           ReadParam(aMsg, aIter, &aResult->indexes);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.name, aLog);
    LogParam(aParam.id, aLog);
    LogParam(aParam.keyPath, aLog);
    LogParam(aParam.autoIncrement, aLog);
    LogParam(aParam.indexes, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::DatabaseInfoGuts>
{
  typedef mozilla::dom::indexedDB::DatabaseInfoGuts paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.name);
    WriteParam(aMsg, aParam.origin);
    WriteParam(aMsg, aParam.version);
    WriteParam(aMsg, aParam.nextObjectStoreId);
    WriteParam(aMsg, aParam.nextIndexId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->name) &&
           ReadParam(aMsg, aIter, &aResult->origin) &&
           ReadParam(aMsg, aIter, &aResult->version) &&
           ReadParam(aMsg, aIter, &aResult->nextObjectStoreId) &&
           ReadParam(aMsg, aIter, &aResult->nextIndexId);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.name, aLog);
    LogParam(aParam.origin, aLog);
    LogParam(aParam.version, aLog);
    LogParam(aParam.nextObjectStoreId, aLog);
    LogParam(aParam.nextIndexId, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::IndexUpdateInfo>
{
  typedef mozilla::dom::indexedDB::IndexUpdateInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.indexId);
    WriteParam(aMsg, aParam.indexUnique);
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->indexId) &&
           ReadParam(aMsg, aIter, &aResult->indexUnique) &&
           ReadParam(aMsg, aIter, &aResult->value);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.indexId, aLog);
    LogParam(aParam.indexUnique, aLog);
    LogParam(aParam.value, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::SerializedStructuredCloneReadInfo>
{
  typedef mozilla::dom::indexedDB::SerializedStructuredCloneReadInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.dataLength);
    if (aParam.dataLength) {
      aMsg->WriteBytes(aParam.data, aParam.dataLength);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->dataLength)) {
      return false;
    }

    if (aResult->dataLength) {
      const char** buffer =
        const_cast<const char**>(reinterpret_cast<char**>(&aResult->data));
      if (!aMsg->ReadBytes(aIter, buffer, aResult->dataLength)) {
        return false;
      }
    } else {
      aResult->data = NULL;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.dataLength, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::SerializedStructuredCloneWriteInfo>
{
  typedef mozilla::dom::indexedDB::SerializedStructuredCloneWriteInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.dataLength);
    if (aParam.dataLength) {
      aMsg->WriteBytes(aParam.data, aParam.dataLength);
    }
    WriteParam(aMsg, aParam.offsetToKeyProp);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->dataLength)) {
      return false;
    }

    if (aResult->dataLength) {
      const char** buffer =
        const_cast<const char**>(reinterpret_cast<char**>(&aResult->data));
      if (!aMsg->ReadBytes(aIter, buffer, aResult->dataLength)) {
        return false;
      }
    } else {
      aResult->data = NULL;
    }

    if (!ReadParam(aMsg, aIter, &aResult->offsetToKeyProp)) {
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.dataLength, aLog);
    LogParam(aParam.offsetToKeyProp, aLog);
  }
};

} // namespace IPC

#endif // mozilla_dom_indexeddb_serializationhelpers_h__
