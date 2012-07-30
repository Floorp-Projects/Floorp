/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for rendering objects that manually create subtrees of
 * anonymous content
 */

#ifndef nsIAnonymousContentCreator_h___
#define nsIAnonymousContentCreator_h___

#include "nsQueryFrame.h"
#include "nsIContent.h"
#include "nsStyleContext.h"

class nsBaseContentList;
class nsIFrame;
template <class T, class A> class nsTArray;

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

  struct ContentInfo {
    ContentInfo(nsIContent* aContent) :
      mContent(aContent)
    {}

    ContentInfo(nsIContent* aContent, nsStyleContext* aStyleContext) :
      mContent(aContent), mStyleContext(aStyleContext)
    {}

    nsIContent* mContent;
    nsRefPtr<nsStyleContext> mStyleContext;
  };

  /**
   * Creates "native" anonymous content and adds the created content to
   * the aElements array. None of the returned elements can be nullptr.
   *
   * If the anonymous content creator sets the editable flag on some
   * of the elements that it creates, the flag will be applied to the node
   * upon being bound to the document.
   *
   * @note The returned elements are owned by this object. This object is
   *       responsible for calling UnbindFromTree on the elements it returned
   *       from CreateAnonymousContent when appropriate (i.e. before releasing
   *       them).
   */
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements)=0;

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
   * By default this method returns nullptr, which means the default frame
   * is created.
   */
  virtual nsIFrame* CreateFrameFor(nsIContent* aContent) { return nullptr; }
};

#endif

