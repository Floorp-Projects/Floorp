/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileList_h
#define mozilla_dom_FileList_h

#include <cstdint>
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsCycleCollectionTraversalCallback;
template <class T>
class RefPtr;

namespace mozilla {
class ErrorResult;
namespace dom {

class BlobImpls;
class File;
template <typename T>
class Sequence;

class FileList final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FileList)

  explicit FileList(nsISupports* aParent);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() { return mParent; }

  bool Append(File* aFile);

  bool Remove(uint32_t aIndex);

  void Clear();

  File* Item(uint32_t aIndex) const;

  File* IndexedGetter(uint32_t aIndex, bool& aFound) const;

  uint32_t Length() const { return mFiles.Length(); }

  void ToSequence(Sequence<RefPtr<File>>& aSequence, ErrorResult& aRv) const;

 private:
  ~FileList();

  FallibleTArray<RefPtr<File>> mFiles;
  nsCOMPtr<nsISupports> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileList_h
