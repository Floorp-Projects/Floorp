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

  // These tags are used only for main thread structured clone.
  SCTAG_DOM_BLOB,

  // This tag is obsolete and exists only for backwards compatibility with
  // existing IndexedDB databases.
  SCTAG_DOM_FILE_WITHOUT_LASTMODIFIEDDATE,

  SCTAG_DOM_FILELIST,
  SCTAG_DOM_MUTABLEFILE,
  SCTAG_DOM_FILE,

  // These tags are used for both main thread and workers.
  SCTAG_DOM_IMAGEDATA,
  SCTAG_DOM_MAP_MESSAGEPORT,

  SCTAG_DOM_FUNCTION,

  // This tag is for WebCrypto keys
  SCTAG_DOM_WEBCRYPTO_KEY,

  SCTAG_DOM_MAX
};

} // namespace dom
} // namespace mozilla

#endif // StructuredCloneTags_h__
