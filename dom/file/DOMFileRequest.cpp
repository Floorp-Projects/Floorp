/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMFileRequest.h"

#include "mozilla/dom/FileRequestBinding.h"
#include "LockedFile.h"

USING_FILE_NAMESPACE

DOMFileRequest::DOMFileRequest(nsPIDOMWindow* aWindow)
  : FileRequest(aWindow)
{
}

// static
already_AddRefed<DOMFileRequest>
DOMFileRequest::Create(nsPIDOMWindow* aOwner, LockedFile* aLockedFile)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<DOMFileRequest> request = new DOMFileRequest(aOwner);
  request->mLockedFile = aLockedFile;

  return request.forget();
}

/* virtual */ JSObject*
DOMFileRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FileRequestBinding::Wrap(aCx, aScope, this);
}

nsIDOMLockedFile*
DOMFileRequest::GetLockedFile() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mLockedFile;
}
