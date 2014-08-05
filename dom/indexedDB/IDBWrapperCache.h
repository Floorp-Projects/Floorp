/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbwrappercache_h__
#define mozilla_dom_indexeddb_idbwrappercache_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/indexedDB/IndexedDatabase.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBWrapperCache : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   IDBWrapperCache,
                                                   DOMEventTargetHelper)

  JSObject* GetScriptOwner() const
  {
    return mScriptOwner;
  }
  void SetScriptOwner(JSObject* aScriptOwner);

#ifdef DEBUG
  void AssertIsRooted() const;
#else
  inline void AssertIsRooted() const
  {
  }
#endif

protected:
  explicit IDBWrapperCache(DOMEventTargetHelper* aOwner)
    : DOMEventTargetHelper(aOwner), mScriptOwner(nullptr)
  { }
  explicit IDBWrapperCache(nsPIDOMWindow* aOwner)
    : DOMEventTargetHelper(aOwner), mScriptOwner(nullptr)
  { }

  virtual ~IDBWrapperCache();

private:
  JS::Heap<JSObject*> mScriptOwner;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbwrappercache_h__
