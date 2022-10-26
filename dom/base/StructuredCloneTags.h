/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StructuredCloneTags_h__
#define StructuredCloneTags_h__

#include "js/StructuredClone.h"

namespace mozilla::dom {

// CHANGING THE ORDER/PLACEMENT OF EXISTING ENUM VALUES MAY BREAK INDEXEDDB.
// PROCEED WITH EXTREME CAUTION.
//
// If you are planning to add new tags which could be used by IndexedDB,
// consider to use empty slots. See EMPTY_SLOT_x
enum StructuredCloneTags : uint32_t {
  SCTAG_BASE = JS_SCTAG_USER_MIN,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_BLOB,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  // This tag is obsolete and exists only for backwards compatibility with
  // existing IndexedDB databases.
  SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_FILELIST,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_MUTABLEFILE,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_FILE,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_WASM_MODULE,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_IMAGEDATA,

  SCTAG_DOM_DOMPOINT,
  SCTAG_DOM_DOMPOINTREADONLY,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  // This tag is for WebCrypto keys
  SCTAG_DOM_CRYPTOKEY,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_NULL_PRINCIPAL,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_SYSTEM_PRINCIPAL,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_CONTENT_PRINCIPAL,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMQUAD,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_RTCCERTIFICATE,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMRECT,
  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMRECTREADONLY,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_EXPANDED_PRINCIPAL,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMMATRIX,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_URLSEARCHPARAMS,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMMATRIXREADONLY,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_DOMEXCEPTION,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  EMPTY_SLOT_9,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_STRUCTUREDCLONETESTER,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_FILESYSTEMHANDLE,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_FILESYSTEMFILEHANDLE,

  // IMPORTANT: Don't change the order of these enum values. You could break
  // IDB.
  SCTAG_DOM_FILESYSTEMDIRECTORYHANDLE,

  // If you are planning to add new tags which could be used by IndexedDB,
  // consider to use an empty slot. See EMPTY_SLOT_x

  // Please update the static assertions in StructuredCloneHolder.cpp and in
  // IDBObjectStore.cpp, method CommonStructuredCloneReadCallback.

  // --------------------------------------------------------------------------

  // All the following tags are not written to disk and they are not used by
  // IndexedDB directly or via
  // StructuredCloneHolder::{Read,Write}FullySerializableObjects. In theory they
  // can be 'less' stable.

  // Principal written out by worker threads when serializing objects. When
  // reading on the main thread this principal will be converted to a normal
  // principal object using nsJSPrincipals::AutoSetActiveWorkerPrincipal.
  SCTAG_DOM_WORKER_PRINCIPAL,

  SCTAG_DOM_IMAGEBITMAP,
  SCTAG_DOM_MAP_MESSAGEPORT,
  SCTAG_DOM_FORMDATA,

  // This tag is for OffscreenCanvas.
  SCTAG_DOM_CANVAS,

  SCTAG_DOM_DIRECTORY,

  SCTAG_DOM_INPUTSTREAM,

  SCTAG_DOM_STRUCTURED_CLONE_HOLDER,

  SCTAG_DOM_BROWSING_CONTEXT,

  SCTAG_DOM_CLONED_ERROR_OBJECT,

  SCTAG_DOM_READABLESTREAM,

  SCTAG_DOM_WRITABLESTREAM,

  SCTAG_DOM_TRANSFORMSTREAM,

  // IMPORTANT: If you plan to add an new IDB tag, it _must_ be add before the
  // "less stable" tags!
};

}  // namespace mozilla::dom

#endif  // StructuredCloneTags_h__
