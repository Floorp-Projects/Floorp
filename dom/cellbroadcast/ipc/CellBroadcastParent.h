/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_dom_cellbroadcast_CellBroadcastParent_h
#define mozilla_dom_cellbroadcast_CellBroadcastParent_h

#include "mozilla/dom/cellbroadcast/PCellBroadcastParent.h"
#include "nsICellBroadcastService.h"

namespace mozilla {
namespace dom {
namespace cellbroadcast {

class CellBroadcastParent MOZ_FINAL : public PCellBroadcastParent
                                    , public nsICellBroadcastListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICELLBROADCASTLISTENER

  bool Init();

private:
  // MOZ_FINAL suppresses -Werror,-Wdelete-non-virtual-dtor
  ~CellBroadcastParent() {};

  virtual void ActorDestroy(ActorDestroyReason aWhy);
};

} // namespace cellbroadcast
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cellbroadcast_CellBroadcastParent_h