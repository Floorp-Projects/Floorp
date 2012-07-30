/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
class nsImageFrame;

class nsImageMap : public nsStubMutationObserver,
                   public nsIDOMEventListener
{
public:
  nsImageMap();

  nsresult Init(nsImageFrame* aImageFrame, nsIContent* aMap);

  /**
   * Return the first area element (in content order) for the given aX,aY pixel
   * coordinate or nullptr if the coordinate is outside all areas.
   */
  nsIContent* GetArea(nscoord aX, nscoord aY) const;

  /**
   * Return area elements count associated with the image map.
   */
  PRUint32 AreaCount() const { return mAreas.Length(); }

  /**
   * Return area element at the given index.
   */
  nsIContent* GetAreaAt(PRUint32 aIndex) const;

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
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED  

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

  nsImageFrame* mImageFrame;  // the frame that owns us
  nsCOMPtr<nsIContent> mMap;
  nsAutoTArray<Area*, 8> mAreas; // almost always has some entries
  bool mContainsBlockContents;
};

#endif /* nsImageMap_h */
