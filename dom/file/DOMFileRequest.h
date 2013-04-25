/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_DOMFileRequest_h
#define mozilla_dom_file_DOMFileRequest_h

#include "FileRequest.h"

class nsIDOMLockedFile;

BEGIN_FILE_NAMESPACE

class DOMFileRequest : public FileRequest
{
public:
  DOMFileRequest(nsIDOMWindow* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsIDOMLockedFile* GetLockedFile() const;
  IMPL_EVENT_HANDLER(progress)
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_DOMFileRequest_h
