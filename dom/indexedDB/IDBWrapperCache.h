/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbwrappercache_h__
#define mozilla_dom_indexeddb_idbwrappercache_h__

#include "js/RootingAPI.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace indexedDB {

class IDBWrapperCache : public DOMEventTargetHelper
{
  JS::Heap<JSObject*> mScriptOwner;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBWrapperCache,
                                                         DOMEventTargetHelper)

  JSObject*
  GetScriptOwner() const
  {
    return mScriptOwner;
  }

  void
  SetScriptOwner(JSObject* aScriptOwner);

  void AssertIsRooted() const
#ifdef DEBUG
  ;
#else
  { }
#endif

protected:
  explicit IDBWrapperCache(DOMEventTargetHelper* aOwner);
  explicit IDBWrapperCache(nsPIDOMWindow* aOwner);

  virtual ~IDBWrapperCache();
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbwrappercache_h__
