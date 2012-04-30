/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef COMPATIBILITY_MANAGER_H
#define COMPATIBILITY_MANAGER_H

#include "prtypes.h"

class nsAccessNodeWrap;

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
   * Initialize compatibility mode. Called by nsAccessNodeWrap during
   * accessibility initialization.
   */
  static void Init();
  friend class nsAccessNodeWrap;

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
  static PRUint32 sConsumers;
};

} // a11y namespace
} // mozilla namespace

#endif
