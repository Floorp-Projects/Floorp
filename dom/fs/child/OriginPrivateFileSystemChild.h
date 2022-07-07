/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
#define DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_

#include "mozilla/dom/POriginPrivateFileSystemChild.h"
#include "nsISupports.h"

namespace mozilla::dom {

class OriginPrivateFileSystemChild : public POriginPrivateFileSystemChild {
  NS_INLINE_DECL_REFCOUNTING(OriginPrivateFileSystemChild);

 protected:
  virtual ~OriginPrivateFileSystemChild() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_ORIGINPRIVATEFILESYSTEMCHILD_H_
