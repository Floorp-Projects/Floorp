/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMFileSystem.h"
#include "mozilla/dom/DOMFileSystemBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMFileSystem, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMFileSystem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMFileSystem)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMFileSystem)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMFileSystem::DOMFileSystem(nsIGlobalObject* aGlobal)
  : mParent(aGlobal)
{}

DOMFileSystem::~DOMFileSystem()
{}

JSObject*
DOMFileSystem::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMFileSystemBinding::Wrap(aCx, this, aGivenProto);
}

} // dom namespace
} // mozilla namespace
