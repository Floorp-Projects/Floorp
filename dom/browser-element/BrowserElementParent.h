/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BrowserElementHelpers_h
#define mozilla_BrowserElementHelpers_h

#include "nsAString.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "Units.h"
#include "mozilla/dom/Element.h"

class nsIDOMWindow;
class nsIURI;

namespace mozilla {

namespace dom {
class TabParent;
} // namespace dom

namespace layers {
struct TextureFactoryIdentifier;
} // namespace layers

namespace layout {
class PRenderFrameParent;
} // namespace layout

/**
 * BrowserElementParent implements a portion of the parent-process side of
 * <iframe mozbrowser>.
 *
 * Most of the parent-process side of <iframe mozbrowser> is implemented in
 * BrowserElementParent.js.  This file implements the few parts of this
 * functionality which must be written in C++.
 *
 * We don't communicate with the JS code that lives in BrowserElementParent.js;
 * the JS and C++ parts are completely separate.
 */
class BrowserElementParent
{
public:

  /**
   * Possible results from a window.open call.
   * ADDED     - The frame was added to a document (i.e. handled by the embedder).
   * IGNORED   - The frame was not added to a document and the embedder didn't
   *             call preventDefault() to prevent the platform from handling the call.
   * CANCELLED - The frame was not added to a document, but the embedder still
   *             called preventDefault() to prevent the platform from handling the call.
   */

  enum OpenWindowResult {
    OPEN_WINDOW_ADDED,
    OPEN_WINDOW_IGNORED,
    OPEN_WINDOW_CANCELLED
  };

  /**
   * Handle a window.open call from an out-of-process <iframe mozbrowser>.
   *
   * window.open inside <iframe mozbrowser> doesn't actually open a new
   * top-level window.  Instead, the "embedder" (the document which contains
   * the <iframe mozbrowser> whose content called window.open) gets the
   * opportunity to place a new <iframe mozbrowser> in the DOM somewhere.  This
   * new "popup" iframe acts as the opened window.
   *
   * This method proceeds in three steps.
   *
   * 1) We fire a mozbrowseropenwindow CustomEvent on the opener
   *    iframe element.  This event's detail is an instance of
   *    OpenWindowEventDetail.
   *
   * 2) The embedder (the document which contains the opener iframe) can accept
   *    the window.open request by inserting event.detail.frameElement (an iframe
   *    element) into the DOM somewhere.
   *
   * 3) If the embedder accepted the window.open request, we return true and
   *    set aPopupTabParent's frame element to event.detail.frameElement.
   *    Otherwise, we return false.
   *
   * @param aURL the URL the new window should load.  The empty string is
   *             allowed.
   * @param aOpenerTabParent the TabParent whose TabChild called window.open.
   * @param aPopupTabParent the TabParent inside which the opened window will
   *                        live.
   * @return an OpenWindowresult that describes whether the embedder added the
   *         frame to a document and whether it called preventDefault to prevent
   *         the platform from handling the open request.
   */
  static OpenWindowResult
  OpenWindowOOP(dom::TabParent* aOpenerTabParent,
                dom::TabParent* aPopupTabParent,
                layout::PRenderFrameParent* aRenderFrame,
                const nsAString& aURL,
                const nsAString& aName,
                const nsAString& aFeatures,
                layers::TextureFactoryIdentifier* aTextureFactoryIdentifier,
                uint64_t* aLayersId);

  /**
   * Handle a window.open call from an in-process <iframe mozbrowser>.
   *
   * (These parameter types are silly, but they match what our caller has in
   * hand.  Feel free to add an override, if they are inconvenient to you.)
   *
   * @param aURI the URI the new window should load.  May be null.
   * @return an OpenWindowResult that describes whether the browser added the
   *         frame to a document or whether they called preventDefault to prevent
   *         the platform from handling the open request
   */
  static OpenWindowResult
  OpenWindowInProcess(nsPIDOMWindowOuter* aOpenerWindow,
                      nsIURI* aURI,
                      const nsAString& aName,
                      const nsACString& aFeatures,
                      bool aForceNoOpener,
                      mozIDOMWindowProxy** aReturnWindow);

private:
  static OpenWindowResult
  DispatchOpenWindowEvent(dom::Element* aOpenerFrameElement,
                          dom::Element* aPopupFrameElement,
                          const nsAString& aURL,
                          const nsAString& aName,
                          const nsAString& aFeatures);
};

} // namespace mozilla

#endif
