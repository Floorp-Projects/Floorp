/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectoryReader.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DirectoryReader, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DirectoryReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DirectoryReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DirectoryReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DirectoryReader::DirectoryReader(nsIGlobalObject* aGlobal)
  : mParent(aGlobal)
{}

DirectoryReader::~DirectoryReader()
{}

JSObject*
DirectoryReader::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DirectoryReaderBinding::Wrap(aCx, this, aGivenProto);
}

} // dom namespace
} // mozilla namespace
