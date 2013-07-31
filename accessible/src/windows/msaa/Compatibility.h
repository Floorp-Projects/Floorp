/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMPATIBILITY_MANAGER_H
#define COMPATIBILITY_MANAGER_H

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
   * Return true if IAccessible2 disabled.
   */
  static bool IsIA2Off() { return !!(sConsumers & OLDJAWS); }

  /**
   * Return true if JAWS mode is enabled.
   */
  static bool IsJAWS() { return !!(sConsumers & (JAWS | OLDJAWS)); }

  /**
   * Return true if WE mode is enabled.
   */
  static bool IsWE() { return !!(sConsumers & WE); }

  /**
   * Return true if Dolphin mode is enabled.
   */
  static bool IsDolphin() { return !!(sConsumers & DOLPHIN); }

private:
  Compatibility();
  Compatibility(const Compatibility&);
  Compatibility& operator = (const Compatibility&);

  /**
   * Initialize compatibility mode. Called by platform (see Platform.h) during
   * accessibility initialization.
   */
  static void Init();
  friend void PlatformInit();

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

private:
  static uint32_t sConsumers;
};

} // a11y namespace
} // mozilla namespace

#endif
