/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelemetryScrollProbe_h
#define mozilla_dom_TelemetryScrollProbe_h

#include "nsIDOMEventListener.h"

class nsIDocument;
class nsIDOMEvent;
class nsIPresShell;
class nsIWebNavigation;

namespace mozilla {
namespace dom {

class TabChildGlobal;

/*
 * A class for implementing a telemetry scroll probe, listens to
 * pagehide events in a frame script context and records the
 * max scroll y, and amount of vertical scrolling that ocurred.
 * for more information see bug 1340904
 */
class TelemetryScrollProbe final : public nsIDOMEventListener
{
public:
  static void Create(TabChildGlobal* aWebFrame);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  explicit TelemetryScrollProbe(nsWeakPtr aWebNav)
      : mWebNav(aWebNav)
  { }
  ~TelemetryScrollProbe() {}

  already_AddRefed<nsIWebNavigation> GetWebNavigation() const;
  already_AddRefed<nsIDocument> GetDocument() const;
  already_AddRefed<nsIPresShell> GetPresShell() const;

  bool ShouldIgnore(nsIDOMEvent* aEvent) const;

  nsWeakPtr mWebNav;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TelemetryScrollProbe_h
