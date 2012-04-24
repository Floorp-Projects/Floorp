/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_imagedata_h__
#define mozilla_dom_workers_imagedata_h__

#include "Workers.h"

BEGIN_WORKERS_NAMESPACE

namespace imagedata {

bool
InitClass(JSContext* aCx, JSObject* aGlobal);

JSObject*
Create(JSContext* aCx, uint32_t aWidth, uint32_t aHeight, JSObject* aData);

/*
 * All data members live in private slots on the JS Object. Callers must
 * first check IsImageData, after which they may call the data accessors.
 */

bool
IsImageData(JSObject* aObj);

uint32_t
GetWidth(JSObject* aObj);

uint32_t
GetHeight(JSObject* aObj);

JSObject*
GetData(JSObject* aObj);

} // namespace imagedata

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_imagedata_h__
