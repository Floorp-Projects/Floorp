/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Neutering_h
#define mozilla_ipc_Neutering_h

#include "mozilla/GuardObjects.h"

/**
 * This header declares RAII wrappers for Window neutering. See
 * WindowsMessageLoop.cpp for more details.
 */

namespace mozilla {
namespace ipc {

/**
 * This class is a RAII wrapper around Window neutering. As long as a
 * NeuteredWindowRegion object is instantiated, Win32 windows belonging to the
 * current thread will be neutered. It is safe to nest multiple instances of
 * this class.
 */
class MOZ_RAII NeuteredWindowRegion
{
public:
  explicit NeuteredWindowRegion(bool aDoNeuter MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~NeuteredWindowRegion();

  /**
   * This function clears any backlog of nonqueued messages that are pending for
   * the current thread.
   */
  void PumpOnce();

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mNeuteredByThis;
};

/**
 * This class is analagous to MutexAutoUnlock for Mutex; it is an RAII class
 * that is to be instantiated within a NeuteredWindowRegion, thus temporarily
 * disabling neutering for the remainder of its enclosing block.
 * @see NeuteredWindowRegion
 */
class MOZ_RAII DeneuteredWindowRegion
{
public:
  explicit DeneuteredWindowRegion(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  ~DeneuteredWindowRegion();

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mReneuter;
};

class MOZ_RAII SuppressedNeuteringRegion
{
public:
  explicit SuppressedNeuteringRegion(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  ~SuppressedNeuteringRegion();

  static inline bool IsNeuteringSuppressed() { return sSuppressNeutering; }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mReenable;

  static bool sSuppressNeutering;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_Neutering_h

