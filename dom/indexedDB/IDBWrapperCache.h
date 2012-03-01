/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbwrappercache_h__
#define mozilla_dom_indexeddb_idbwrappercache_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsDOMEventTargetHelper.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBWrapperCache : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   IDBWrapperCache,
                                                   nsDOMEventTargetHelper)

  JSObject* GetScriptOwner() const
  {
    return mScriptOwner;
  }
  bool SetScriptOwner(JSObject* aScriptOwner);

  nsIScriptContext* GetScriptContext() const
  {
    return mScriptContext;
  }

  nsPIDOMWindow* GetOwner() const
  {
    return mOwner;
  }

  JSObject* GetParentObject()
  {
    if (mScriptOwner) {
      return mScriptOwner;
    }

    // Do what nsEventTargetSH::PreCreate does.
    nsCOMPtr<nsIScriptGlobalObject> parent;
    nsDOMEventTargetHelper::GetParentObject(getter_AddRefs(parent));

    return parent ? parent->GetGlobalJSObject() : nsnull;
  }

  static IDBWrapperCache* FromSupports(nsISupports* aSupports)
  {
    return static_cast<IDBWrapperCache*>(
      nsDOMEventTargetHelper::FromSupports(aSupports));
  }

protected:
  IDBWrapperCache()
  : mScriptOwner(nsnull)
  { }

  virtual ~IDBWrapperCache();

private:
  JSObject* mScriptOwner;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbwrappercache_h__
