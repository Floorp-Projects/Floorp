/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBrowserElement_h
#define nsBrowserElement_h

#include "mozilla/dom/BindingDeclarations.h"

#include "nsCOMPtr.h"
#include "nsIBrowserElementAPI.h"

class nsFrameLoader;

namespace mozilla {

namespace dom {
class Promise;
}  // namespace dom

class ErrorResult;

/**
 * A helper class for browser-element frames
 */
class nsBrowserElement {
 public:
  nsBrowserElement() = default;
  virtual ~nsBrowserElement() = default;

  void SendMouseEvent(const nsAString& aType, uint32_t aX, uint32_t aY,
                      uint32_t aButton, uint32_t aClickCount,
                      uint32_t aModifiers, ErrorResult& aRv);
  void GoBack(ErrorResult& aRv);
  void GoForward(ErrorResult& aRv);
  void Reload(bool aHardReload, ErrorResult& aRv);
  void Stop(ErrorResult& aRv);

  already_AddRefed<dom::Promise> GetCanGoBack(ErrorResult& aRv);
  already_AddRefed<dom::Promise> GetCanGoForward(ErrorResult& aRv);

 protected:
  virtual already_AddRefed<nsFrameLoader> GetFrameLoader() = 0;

  void InitBrowserElementAPI();
  void DestroyBrowserElementFrameScripts();
  nsCOMPtr<nsIBrowserElementAPI> mBrowserElementAPI;

 private:
  bool IsBrowserElementOrThrow(ErrorResult& aRv);
};

}  // namespace mozilla

#endif  // nsBrowserElement_h
