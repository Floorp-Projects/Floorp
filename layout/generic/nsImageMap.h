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
#ifndef nsImageMap_h___
#define nsImageMap_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsVoidArray.h"
#include "nsStubDocumentObserver.h"
#include "nsIDOMFocusListener.h"
#include "nsIFrame.h"
#include "nsIImageMap.h"
#include "nsIHTMLContent.h"

class nsIDOMHTMLAreaElement;
class nsIDOMHTMLMapElement;
class nsPresContext;
class nsIRenderingContext;
class nsIURI;
class nsString;
class nsIDOMEvent;

class nsImageMap : public nsStubDocumentObserver, public nsIDOMFocusListener,
                   public nsIImageMap
{
public:
  nsImageMap();

  nsresult Init(nsIPresShell* aPresShell, nsIFrame* aImageFrame, nsIDOMHTMLMapElement* aMap);

  /**
   * See if the given aX,aY <b>pixel</b> coordinates are in the image
   * map. If they are then PR_TRUE is returned and aContent points to the
   * found area. If the coordinates are not in the map then PR_FALSE
   * is returned.
   */
  PRBool IsInside(nscoord aX, nscoord aY,
                  nsIContent** aContent) const;

  /**
   * See if the given aX,aY <b>pixel</b> coordinates are in the image
   * map.
   */
  PRBool IsInside(nscoord aX, nscoord aY) const;

  void Draw(nsPresContext* aCX, nsIRenderingContext& aRC);
  
  /** 
   * Called just before the nsImageFrame releases us. 
   * Used to break the cycle caused by the DOM listener.
   */
  void Destroy(void);
  
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDocumentObserver
  virtual void AttributeChanged(nsIDocument* aDocument, nsIContent* aContent,
                                PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                PRInt32 aModType);
  virtual void ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer);
  virtual void ContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                               nsIContent* aChild, PRInt32 aIndexInContainer);
  virtual void ContentRemoved(nsIDocument* aDocument, nsIContent* aContainer,
                              nsIContent* aChild, PRInt32 aIndexInContainer);

  //nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  //nsIImageMap
  NS_IMETHOD GetBoundsForAreaContent(nsIContent *aContent, 
                                     nsPresContext* aPresContext, 
                                     nsRect& aBounds);

protected:
  virtual ~nsImageMap();

  void FreeAreas();

  nsresult UpdateAreas();
  nsresult UpdateAreasForBlock(nsIContent* aParent, PRBool* aFoundAnchor);

  static PRBool IsAncestorOf(nsIContent* aContent,
                             nsIContent* aAncestorContent);

  nsresult AddArea(nsIContent* aArea);
 
  nsresult ChangeFocus(nsIDOMEvent* aEvent, PRBool aFocus);

  void MaybeUpdateAreas(nsIContent *aContent);

  nsIPresShell* mPresShell; // WEAK - owns the frame that owns us
  nsIFrame* mImageFrame;  // the frame that owns us
  nsIDocument* mDocument; // WEAK - the imagemap will not outlive the document
  nsCOMPtr<nsIHTMLContent> mMap;
  nsAutoVoidArray mAreas; // almost always has some entries
  PRBool mContainsBlockContents;
};

#endif /* nsImageMap_h___ */
