/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NuwaChild_h
#define mozilla_dom_NuwaChild_h

#include "mozilla/Assertions.h"
#include "mozilla/dom/PNuwaChild.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
class NuwaChild: public mozilla::dom::PNuwaChild
{
public:
  virtual bool RecvFork() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override
  { }

  static NuwaChild* GetSingleton();

private:
  static NuwaChild* sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_NuwaChild_h
