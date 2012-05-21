/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StructuredCloneTags_h__
#define StructuredCloneTags_h__

#include "jsapi.h"

namespace mozilla {
namespace dom {

enum StructuredCloneTags {
  SCTAG_BASE = JS_SCTAG_USER_MIN,

  // These tags are used only for main thread structured clone.
  SCTAG_DOM_BLOB,
  SCTAG_DOM_FILE,
  SCTAG_DOM_FILELIST,

  // These tags are used for both main thread and workers.
  SCTAG_DOM_IMAGEDATA,

  SCTAG_DOM_MAX
};

} // namespace dom
} // namespace mozilla

#endif // StructuredCloneTags_h__
