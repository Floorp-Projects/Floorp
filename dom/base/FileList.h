/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileList_h
#define mozilla_dom_FileList_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMFileList.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class BlobImpls;
class File;

class FileList final : public nsIDOMFileList,
                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileList)

  NS_DECL_NSIDOMFILELIST

  explicit FileList(nsISupports* aParent)
    : mParent(aParent)
  {}

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(File* aFile);
  void Append(Directory* aDirectory);

  bool Remove(uint32_t aIndex)
  {
    if (aIndex < mFilesOrDirectories.Length()) {
      mFilesOrDirectories.RemoveElementAt(aIndex);
      return true;
    }

    return false;
  }

  void Clear()
  {
    return mFilesOrDirectories.Clear();
  }

  static FileList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMFileList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMFileList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMFileList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<FileList*>(aSupports);
  }

  const OwningFileOrDirectory& UnsafeItem(uint32_t aIndex) const
  {
    MOZ_ASSERT(aIndex < Length());
    return mFilesOrDirectories[aIndex];
  }

  void Item(uint32_t aIndex,
            Nullable<OwningFileOrDirectory>& aFileOrDirectory,
            ErrorResult& aRv) const;

  void IndexedGetter(uint32_t aIndex, bool& aFound,
                     Nullable<OwningFileOrDirectory>& aFileOrDirectory,
                     ErrorResult& aRv) const;

  uint32_t Length() const
  {
    return mFilesOrDirectories.Length();
  }

  void ToSequence(Sequence<OwningFileOrDirectory>& aSequence,
                  ErrorResult& aRv) const;

  bool ClonableToDifferentThreadOrProcess() const;

private:
  ~FileList() {}

  nsTArray<OwningFileOrDirectory> mFilesOrDirectories;
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileList_h
