/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenManagerParent_h
#define mozilla_dom_ScreenManagerParent_h

#include "mozilla/dom/PScreenManagerParent.h"
#include "nsIScreenManager.h"

namespace mozilla {
namespace dom {

class ScreenManagerParent : public PScreenManagerParent
{
 public:
  ScreenManagerParent(uint32_t* aNumberOfScreens,
                      float* aSystemDefaultScale,
                      bool* aSuccess);
  ~ScreenManagerParent() {};

  virtual bool RecvRefresh(uint32_t* aNumberOfScreens,
                           float* aSystemDefaultScale,
                           bool* aSuccess) override;

  virtual bool RecvScreenRefresh(const uint32_t& aId,
                                 ScreenDetails* aRetVal,
                                 bool* aSuccess) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvGetPrimaryScreen(ScreenDetails* aRetVal,
                                    bool* aSuccess) override;

  virtual bool RecvScreenForRect(const int32_t& aLeft,
                                 const int32_t& aTop,
                                 const int32_t& aWidth,
                                 const int32_t& aHeight,
                                 ScreenDetails* aRetVal,
                                 bool* aSuccess) override;

  virtual bool RecvScreenForBrowser(const TabId& aTabId,
                                    ScreenDetails* aRetVal,
                                    bool* aSuccess) override;

 private:
  bool ExtractScreenDetails(nsIScreen* aScreen, ScreenDetails &aDetails);
  nsCOMPtr<nsIScreenManager> mScreenMgr;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScreenManagerParent_h
