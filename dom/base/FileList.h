/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileList_h
#define mozilla_dom_FileList_h

#include "mozilla/dom/BindingDeclarations.h"
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

  bool Append(File* aFile)
  {
    return mFiles.AppendElement(aFile);
  }

  bool Remove(uint32_t aIndex)
  {
    if (aIndex < mFiles.Length()) {
      mFiles.RemoveElementAt(aIndex);
      return true;
    }

    return false;
  }

  void Clear()
  {
    return mFiles.Clear();
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

  File* Item(uint32_t aIndex)
  {
    return mFiles.SafeElementAt(aIndex);
  }

  File* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < mFiles.Length();
    if (!aFound) {
      return nullptr;
    }
    return mFiles.ElementAt(aIndex);
  }

  uint32_t Length()
  {
    return mFiles.Length();
  }

private:
  ~FileList() {}

  nsTArray<RefPtr<File>> mFiles;
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileList_h
