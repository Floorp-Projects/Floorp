/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileEntry.h"

namespace mozilla {
namespace dom {

FileEntry::FileEntry(nsIGlobalObject* aGlobal)
  : Entry(aGlobal)
{}

FileEntry::~FileEntry()
{}

JSObject*
FileEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileEntryBinding::Wrap(aCx, this, aGivenProto);
}

} // dom namespace
} // mozilla namespace
