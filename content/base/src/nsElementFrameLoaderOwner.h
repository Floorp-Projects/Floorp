/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsElementFrameLoaderOwner_h
#define nsElementFrameLoaderOwner_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "nsIFrameLoader.h"
#include "nsIDOMEventListener.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/ErrorResult.h"

#include "nsFrameLoader.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

class nsXULElement;

/**
 * A helper class for frame elements
 */
class nsElementFrameLoaderOwner : public nsIFrameLoaderOwner
{
public:
  nsElementFrameLoaderOwner(mozilla::dom::FromParser aFromParser)
    : mNetworkCreated(aFromParser == mozilla::dom::FROM_PARSER_NETWORK)
    , mBrowserFrameListenersRegistered(false)
    , mFrameLoaderCreationDisallowed(false)
  {
  }

  virtual ~nsElementFrameLoaderOwner();

  NS_DECL_NSIFRAMELOADEROWNER

  // nsIContent
  void SwapFrameLoaders(nsXULElement& aOtherOwner, mozilla::ErrorResult& aError);

protected:
  // This doesn't really ensure a frame loader in all cases, only when
  // it makes sense.
  void EnsureFrameLoader();
  nsresult LoadSrc();
  nsIDocument* GetContentDocument();
  nsresult GetContentDocument(nsIDOMDocument** aContentDocument);
  already_AddRefed<nsPIDOMWindow> GetContentWindow();
  nsresult GetContentWindow(nsIDOMWindow** aContentWindow);

  /**
   * Get element for this frame. Avoids diamond inheritance problem.
   * @return Element for this node
   */
  virtual mozilla::dom::Element* ThisFrameElement() = 0;

  nsRefPtr<nsFrameLoader> mFrameLoader;

  /**
   * True when the element is created by the parser using the
   * NS_FROM_PARSER_NETWORK flag.
   * If the element is modified, it may lose the flag.
   */
  bool mNetworkCreated;

  bool mBrowserFrameListenersRegistered;
  bool mFrameLoaderCreationDisallowed;
};

#endif // nsElementFrameLoaderOwner_h
