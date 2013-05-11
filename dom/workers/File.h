/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_file_h__
#define mozilla_dom_workers_file_h__

#include "Workers.h"

#include "jspubtd.h"

class nsIDOMFile;
class nsIDOMBlob;

BEGIN_WORKERS_NAMESPACE

namespace file {

bool
InitClasses(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

JSObject*
CreateBlob(JSContext* aCx, nsIDOMBlob* aBlob);

nsIDOMBlob*
GetDOMBlobFromJSObject(JSObject* aObj);

JSObject*
CreateFile(JSContext* aCx, nsIDOMFile* aFile);

nsIDOMFile*
GetDOMFileFromJSObject(JSObject* aObj);

} // namespace file

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_file_h__ */
