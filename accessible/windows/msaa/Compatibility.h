/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMPATIBILITY_MANAGER_H
#define COMPATIBILITY_MANAGER_H

#include "mozilla/Maybe.h"
#include "nsString.h"
#include <stdint.h>

namespace mozilla {
namespace a11y {

/**
 * Used to get compatibility modes. Note, modes are computed at accessibility
 * start up time and aren't changed during lifetime.
 */
class Compatibility
{
public:
  /**
   * Return true if JAWS mode is enabled.
   */
  static bool IsJAWS() { return !!(sConsumers & (JAWS | OLDJAWS)); }

  /**
   * Return true if using an e10s incompatible Jaws.
   */
  static bool IsOldJAWS() { return !!(sConsumers & OLDJAWS); }

  /**
   * Return true if WE mode is enabled.
   */
  static bool IsWE() { return !!(sConsumers & WE); }

  /**
   * Return true if Dolphin mode is enabled.
   */
  static bool IsDolphin() { return !!(sConsumers & DOLPHIN); }

  /**
   * @return ID of a11y manifest resource to be passed to
   * mscom::ActivationContext
   */
  static uint16_t GetActCtxResourceId();

  /**
   * Return a string describing sConsumers suitable for about:support.
   * Exposed through nsIXULRuntime.accessibilityInstantiator.
   */
  static void GetHumanReadableConsumersStr(nsAString &aResult);

  /**
   * Initialize compatibility mode information.
   */
  static void Init();

  static Maybe<bool> OnUIAMessage(WPARAM aWParam, LPARAM aLParam);

  static Maybe<DWORD> GetUiaRemotePid() { return sUiaRemotePid; }

  /**
   * return true if a known, non-UIA a11y consumer is present
   */
  static bool HasKnownNonUiaConsumer();

private:
  Compatibility();
  Compatibility(const Compatibility&);
  Compatibility& operator = (const Compatibility&);

  static void InitConsumers();

  /**
   * List of detected consumers of a11y (used for statistics/telemetry and compat)
   */
  enum {
    NVDA = 1 << 0,
    JAWS = 1 << 1,
    OLDJAWS = 1 << 2,
    WE = 1 << 3,
    DOLPHIN = 1 << 4,
    SEROTEK = 1 << 5,
    COBRA = 1 << 6,
    ZOOMTEXT = 1 << 7,
    KAZAGURU = 1 << 8,
    YOUDAO = 1 << 9,
    UNKNOWN = 1 << 10,
    UIAUTOMATION = 1 << 11
  };
  #define CONSUMERS_ENUM_LEN 12

private:
  static uint32_t sConsumers;
  static Maybe<DWORD> sUiaRemotePid;
};

} // a11y namespace
} // mozilla namespace

#endif
