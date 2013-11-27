/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundchildimpl_h__
#define mozilla_ipc_backgroundchildimpl_h__

#include "mozilla/Attributes.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla {
namespace ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundChildImpl : public PBackgroundChild
{
protected:
  BackgroundChildImpl();
  virtual ~BackgroundChildImpl();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBackgroundTestChild*
  AllocPBackgroundTestChild(const nsCString& aTestArg) MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundTestChild(PBackgroundTestChild* aActor) MOZ_OVERRIDE;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundchildimpl_h__
