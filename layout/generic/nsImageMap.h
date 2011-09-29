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

/* code for HTML client-side image maps */

#ifndef nsImageMap_h
#define nsImageMap_h

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsTArray.h"
#include "nsStubMutationObserver.h"
#include "nsIDOMEventListener.h"
#include "nsIFrame.h"

class Area;
class nsIDOMEvent;
class nsRenderingContext;

class nsImageMap : public nsStubMutationObserver,
                   public nsIDOMEventListener
{
public:
  nsImageMap();

  nsresult Init(nsIFrame* aImageFrame, nsIContent* aMap);

  /**
   * See if the given aX,aY <b>pixel</b> coordinates are in the image
   * map. If they are then PR_TRUE is returned and aContent points to the
   * found area. If the coordinates are not in the map then PR_FALSE
   * is returned.
   */
  bool IsInside(nscoord aX, nscoord aY,
                  nsIContent** aContent) const;

  void Draw(nsIFrame* aFrame, nsRenderingContext& aRC);
  
  /** 
   * Called just before the nsImageFrame releases us. 
   * Used to break the cycle caused by the DOM listener.
   */
  void Destroy();
  
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  //nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult GetBoundsForAreaContent(nsIContent *aContent,
                                   nsRect& aBounds);

protected:
  virtual ~nsImageMap();

  void FreeAreas();

  nsresult UpdateAreas();
  nsresult SearchForAreas(nsIContent* aParent, bool& aFoundArea,
                          bool& aFoundAnchor);

  nsresult AddArea(nsIContent* aArea);
 
  void MaybeUpdateAreas(nsIContent *aContent);

  nsIFrame* mImageFrame;  // the frame that owns us
  nsCOMPtr<nsIContent> mMap;
  nsAutoTArray<Area*, 8> mAreas; // almost always has some entries
  bool mContainsBlockContents;
};

#endif /* nsImageMap_h */
