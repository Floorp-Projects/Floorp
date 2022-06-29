/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_BACKGROUNDFILESYSTEMCHILD_H_
#define DOM_FS_CHILD_BACKGROUNDFILESYSTEMCHILD_H_

#include "mozilla/dom/PBackgroundFileSystemChild.h"
#include "nsISupports.h"

namespace mozilla::dom {

class BackgroundFileSystemChild : public PBackgroundFileSystemChild {
  NS_INLINE_DECL_REFCOUNTING(BackgroundFileSystemChild);

 protected:
  virtual ~BackgroundFileSystemChild() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_BACKGROUNDFILESYSTEMCHILD_H_
