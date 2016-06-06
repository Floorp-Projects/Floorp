/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Entry_h
#define mozilla_dom_Entry_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class DOMFileSystem;
class OwningFileOrDirectory;

class Entry
  : public nsISupports
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Entry)

  static already_AddRefed<Entry>
  Create(nsIGlobalObject* aGlobalObject,
         const OwningFileOrDirectory& aFileOrDirectory,
         DOMFileSystem* aFileSystem);

  nsIGlobalObject*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool
  IsFile() const
  {
    return false;
  }

  virtual bool
  IsDirectory() const
  {
    return false;
  }

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const = 0;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const = 0;

  DOMFileSystem*
  Filesystem() const
  {
    return mFileSystem;
  }

protected:
  Entry(nsIGlobalObject* aGlobalObject,
        DOMFileSystem* aFileSystem);
  virtual ~Entry();

private:
  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<DOMFileSystem> mFileSystem;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Entry_h
