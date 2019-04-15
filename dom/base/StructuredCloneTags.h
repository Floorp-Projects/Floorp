/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StructuredCloneTags_h__
#define StructuredCloneTags_h__

#include "js/StructuredClone.h"

namespace mozilla {
namespace dom {

// CHANGING THE ORDER/PLACEMENT OF EXISTING ENUM VALUES MAY BREAK INDEXEDDB.
// PROCEED WITH EXTREME CAUTION.
enum StructuredCloneTags {
  SCTAG_BASE = JS_SCTAG_USER_MIN,

  SCTAG_DOM_BLOB,

  // This tag is obsolete and exists only for backwards compatibility with
  // existing IndexedDB databases.
  SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE,

  SCTAG_DOM_FILELIST,
  SCTAG_DOM_MUTABLEFILE,
  SCTAG_DOM_FILE,

  SCTAG_DOM_WASM,

  // New IDB tags go here!

  // These tags are used for both main thread and workers.
  SCTAG_DOM_IMAGEDATA,
  SCTAG_DOM_MAP_MESSAGEPORT,

  SCTAG_DOM_FUNCTION,

  // This tag is for WebCrypto keys
  SCTAG_DOM_WEBCRYPTO_KEY,

  SCTAG_DOM_NULL_PRINCIPAL,
  SCTAG_DOM_SYSTEM_PRINCIPAL,
  SCTAG_DOM_CONTENT_PRINCIPAL,

  SCTAG_DOM_IMAGEBITMAP,

  SCTAG_DOM_RTC_CERTIFICATE,

  SCTAG_DOM_FORMDATA,

  // This tag is for OffscreenCanvas.
  SCTAG_DOM_CANVAS,

  SCTAG_DOM_EXPANDED_PRINCIPAL,

  SCTAG_DOM_DIRECTORY,

  // This tag is used by both main thread and workers.
  SCTAG_DOM_URLSEARCHPARAMS,

  SCTAG_DOM_INPUTSTREAM,

  SCTAG_DOM_STRUCTURED_CLONE_HOLDER,

  // When adding a new tag for IDB, please don't add it to the end of the list!
  // Tags that are supported by IDB must not ever change. See the static assert
  // in IDBObjectStore.cpp, method CommonStructuredCloneReadCallback.
  // Adding to the end of the list would make removing of other tags harder in
  // future.

  SCTAG_DOM_MAX,

  SCTAG_DOM_STRUCTURED_CLONE_TESTER,

  // Principal written out by worker threads when serializing objects. When
  // reading on the main thread this principal will be converted to a normal
  // principal object using nsJSPrincipals::AutoSetActiveWorkerPrincipal.
  SCTAG_DOM_WORKER_PRINCIPAL
};

}  // namespace dom
}  // namespace mozilla

#endif  // StructuredCloneTags_h__
