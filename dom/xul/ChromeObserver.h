/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChromeObserver_h
#define mozilla_dom_ChromeObserver_h

#include "nsStubMutationObserver.h"

class nsIWidget;

namespace mozilla::dom {
class Document;

class ChromeObserver final : public nsStubMutationObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit ChromeObserver(Document* aDocument);
  void Init();

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

 protected:
  nsIWidget* GetWindowWidget();
  void SetDrawsTitle(bool aState);
  void SetChromeMargins(const nsAttrValue* aValue);
  nsresult HideWindowChrome(bool aShouldHide);

 private:
  void ResetChromeMargins();
  ~ChromeObserver();
  // A weak pointer cleared when the element will be destroyed.
  Document* MOZ_NON_OWNING_REF mDocument;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ChromeObserver_h
