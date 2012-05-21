/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_indexeddatabase_h__
#define mozilla_dom_indexeddb_indexeddatabase_h__

#include "nsIProgrammingLanguage.h"

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDOMError.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

#define BEGIN_INDEXEDDB_NAMESPACE \
  namespace mozilla { namespace dom { namespace indexedDB {

#define END_INDEXEDDB_NAMESPACE \
  } /* namespace indexedDB */ } /* namepsace dom */ } /* namespace mozilla */

#define USING_INDEXEDDB_NAMESPACE \
  using namespace mozilla::dom::indexedDB;

class nsIDOMBlob;

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo;

struct StructuredCloneReadInfo {
  void Swap(StructuredCloneReadInfo& aCloneReadInfo) {
    mCloneBuffer.swap(aCloneReadInfo.mCloneBuffer);
    mFileInfos.SwapElements(aCloneReadInfo.mFileInfos);
  }

  JSAutoStructuredCloneBuffer mCloneBuffer;
  nsTArray<nsRefPtr<FileInfo> > mFileInfos;
};

struct StructuredCloneWriteInfo {
  void Swap(StructuredCloneWriteInfo& aCloneWriteInfo) {
    mCloneBuffer.swap(aCloneWriteInfo.mCloneBuffer);
    mBlobs.SwapElements(aCloneWriteInfo.mBlobs);
    mOffsetToKeyProp = aCloneWriteInfo.mOffsetToKeyProp;
  }

  JSAutoStructuredCloneBuffer mCloneBuffer;
  nsTArray<nsCOMPtr<nsIDOMBlob> > mBlobs;
  PRUint64 mOffsetToKeyProp;
};

inline
void
AppendConditionClause(const nsACString& aColumnName,
                      const nsACString& aArgName,
                      bool aLessThan,
                      bool aEquals,
                      nsACString& aResult)
{
  aResult += NS_LITERAL_CSTRING(" AND ") + aColumnName +
             NS_LITERAL_CSTRING(" ");

  if (aLessThan) {
    aResult.AppendLiteral("<");
  }
  else {
    aResult.AppendLiteral(">");
  }

  if (aEquals) {
    aResult.AppendLiteral("=");
  }

  aResult += NS_LITERAL_CSTRING(" :") + aArgName;
}

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_indexeddatabase_h__
