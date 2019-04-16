/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ZoomConstraintsClient_h_
#define ZoomConstraintsClient_h_

#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"

namespace mozilla {
class PresShell;
namespace dom {
class EventTarget;
}  // namespace dom
}  // namespace mozilla

class ZoomConstraintsClient final : public nsIDOMEventListener,
                                    public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  ZoomConstraintsClient();

 private:
  ~ZoomConstraintsClient();

 public:
  void Init(mozilla::PresShell* aPresShell, mozilla::dom::Document* aDocument);
  void Destroy();
  void ScreenSizeChanged();

 private:
  void RefreshZoomConstraints();

  RefPtr<mozilla::dom::Document> mDocument;
  // raw ref since the presShell owns this
  mozilla::PresShell* MOZ_NON_OWNING_REF mPresShell;
  nsCOMPtr<mozilla::dom::EventTarget> mEventTarget;
  mozilla::Maybe<mozilla::layers::ScrollableLayerGuid> mGuid;
};

#endif
