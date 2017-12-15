/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for HTML client-side image maps */

#ifndef nsImageMap_h
#define nsImageMap_h

#include "mozilla/gfx/2D.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsTArray.h"
#include "nsStubMutationObserver.h"
#include "nsIDOMEventListener.h"

class Area;
class nsImageFrame;
class nsIFrame;
class nsIContent;
struct nsRect;

namespace mozilla {
namespace dom {
class HTMLAreaElement;
}
}

class nsImageMap final : public nsStubMutationObserver,
                         public nsIDOMEventListener
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::ColorPattern ColorPattern;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;

public:
  nsImageMap();

  void Init(nsImageFrame* aImageFrame, nsIContent* aMap);

  /**
   * Return the first area element (in content order) for the given aX,aY pixel
   * coordinate or nullptr if the coordinate is outside all areas.
   */
  nsIContent* GetArea(nscoord aX, nscoord aY) const;

  /**
   * Return area elements count associated with the image map.
   */
  uint32_t AreaCount() const { return mAreas.Length(); }

  /**
   * Return area element at the given index.
   */
  nsIContent* GetAreaAt(uint32_t aIndex) const;

  void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
            const ColorPattern& aColor,
            const StrokeOptions& aStrokeOptions = StrokeOptions());

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

  using AreaList = AutoTArray<mozilla::UniquePtr<Area>, 8>;

protected:
  virtual ~nsImageMap();

  void FreeAreas();

  void UpdateAreas();

  void SearchForAreas(nsIContent* aParent);

  void AddArea(mozilla::dom::HTMLAreaElement* aArea);
  void AreaRemoved(mozilla::dom::HTMLAreaElement* aArea);

  void MaybeUpdateAreas(nsIContent *aContent);

  nsImageFrame* mImageFrame;  // the frame that owns us
  nsCOMPtr<nsIContent> mMap;

  // almost always has some entries
  AreaList mAreas;

  // This is set when we search for all area children and tells us whether we
  // should consider the whole subtree or just direct children when we get
  // content notifications about changes inside the map subtree.
  bool mConsiderWholeSubtree;
};

#endif /* nsImageMap_h */
