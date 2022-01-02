/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for rendering objects that manually create subtrees of
 * anonymous content
 */

#ifndef nsIAnonymousContentCreator_h___
#define nsIAnonymousContentCreator_h___

#include "mozilla/AnonymousContentKey.h"

#include "nsQueryFrame.h"
#include "nsTArrayForwardDeclare.h"
#include "X11UndefineNone.h"

class nsIContent;

/**
 * Any source for anonymous content can implement this interface to provide it.
 * HTML frames like nsFileControlFrame currently use this.
 *
 * @see nsCSSFrameConstructor
 */
class nsIAnonymousContentCreator {
 public:
  NS_DECL_QUERYFRAME_TARGET(nsIAnonymousContentCreator)

  struct ContentInfo {
    explicit ContentInfo(
        nsIContent* aContent,
        mozilla::AnonymousContentKey aKey = mozilla::AnonymousContentKey::None)
        : mContent(aContent), mKey(aKey) {}

    nsIContent* mContent;
    mozilla::AnonymousContentKey mKey;
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
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) = 0;

  /**
   * Appends "native" anonymous children created by CreateAnonymousContent()
   * to the given content list depending on the filter.
   *
   * @see nsIContent::GetChildren for set of values used for filter.  Currently,
   *   eSkipPlaceholderContent is the only flag that any implementation of
   *   this method heeds.
   */
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) = 0;
};

#endif
