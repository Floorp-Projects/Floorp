/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentAreaDragDrop_h__
#define nsContentAreaDragDrop_h__

#include "nsCOMPtr.h"

#include "nsITransferable.h"
#include "nsIContentSecurityPolicy.h"

class nsICookieJarSettings;
class nsPIDOMWindowOuter;
class nsITransferable;
class nsIContent;
class nsIFile;

namespace mozilla::dom {
class DataTransfer;
class Selection;
}  // namespace mozilla::dom

//
// class nsContentAreaDragDrop, used to generate the dragdata
//
class nsContentAreaDragDrop {
 public:
  /**
   * Determine what data in the content area, if any, is being dragged.
   *
   * aWindow - the window containing the target node
   * aTarget - the mousedown event target that started the drag
   * aSelectionTargetNode - the node where the drag event should be fired
   * aIsAltKeyPressed - true if the Alt key is pressed. In some cases, this
   *                    will prevent the drag from occuring. For example,
   *                    holding down Alt over a link should select the text,
   *                    not drag the link.
   * aDataTransfer - the dataTransfer for the drag event.
   * aCanDrag - [out] set to true if the drag may proceed, false to stop the
   *            drag entirely
   * aSelection - [out] set to the selection being dragged, or null if no
   *                    selection is being dragged.
   * aDragNode - [out] the link, image or area being dragged, or null if the
   *             drag occurred on another element.
   * aCSP       - [out] set to the CSP of the Drag, or null if
   *                    it's from browser chrome or OS
   * aCookieJarSettings - [out] set to the cookieJarSetting of the Drag, or null
   *                            if it's from browser chrome or OS
   */
  static nsresult GetDragData(nsPIDOMWindowOuter* aWindow, nsIContent* aTarget,
                              nsIContent* aSelectionTargetNode,
                              bool aIsAltKeyPressed,
                              mozilla::dom::DataTransfer* aDataTransfer,
                              bool* aCanDrag,
                              mozilla::dom::Selection** aSelection,
                              nsIContent** aDragNode,
                              nsIContentSecurityPolicy** aCsp,
                              nsICookieJarSettings** aCookieJarSettings);
};

// this is used to save images to disk lazily when the image data is asked for
// during the drop instead of when it is added to the drag data transfer. This
// ensures that the image data is only created when an image drop is allowed.
class nsContentAreaDragDropDataProvider : public nsIFlavorDataProvider {
  virtual ~nsContentAreaDragDropDataProvider() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFLAVORDATAPROVIDER

  nsresult SaveURIToFile(nsIURI* inSourceURI,
                         nsIPrincipal* inTriggeringPrincipal,
                         nsICookieJarSettings* inCookieJarSettings,
                         nsIFile* inDestFile, nsContentPolicyType inPolicyType,
                         bool isPrivate);
};

#endif /* nsContentAreaDragDrop_h__ */
