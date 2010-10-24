/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * interface for rendering objects that manually create subtrees of
 * anonymous content
 */

#ifndef nsIAnonymousContentCreator_h___
#define nsIAnonymousContentCreator_h___

#include "nsQueryFrame.h"
#include "nsIContent.h"

class nsIFrame;
template <class T> class nsTArray;

/**
 * Any source for anonymous content can implement this interface to provide it.
 * HTML frames like nsFileControlFrame currently use this.
 *
 * @see nsCSSFrameConstructor
 */
class nsIAnonymousContentCreator
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIAnonymousContentCreator)

  /**
   * Creates "native" anonymous content and adds the created content to
   * the aElements array. None of the returned elements can be nsnull.
   *
   * @note The returned elements are owned by this object. This object is
   *       responsible for calling UnbindFromTree on the elements it returned
   *       from CreateAnonymousContent when appropriate (i.e. before releasing
   *       them).
   */
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements)=0;

  /**
   * Appends "native" anonymous children created by CreateAnonymousContent()
   * to the given content list depending on the filter.
   *
   * @see nsIContent::GetChildren for set of values used for filter.
   */
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter) = 0;

  /**
   * Implementations can override this method to create special frames for the
   * anonymous content returned from CreateAnonymousContent.
   * By default this method returns nsnull, which means the default frame
   * is created.
   */
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent) { return nsnull; }
};

#endif

