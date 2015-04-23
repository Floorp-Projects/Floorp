/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWinUtils_h_
#define nsWinUtils_h_

#include <windows.h>

#include "nsIDOMCSSStyleDeclaration.h"
#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"

class nsIContent;

namespace mozilla {
namespace a11y {

class DocAccessible;

const LPCWSTR kClassNameRoot = L"MozillaUIWindowClass";
const LPCWSTR kClassNameTabContent = L"MozillaContentWindowClass";

class nsWinUtils
{
public:
  /**
   * Return computed styles declaration for the given node.
   *
   * @note Please use it carefully since it can shutdown the accessible tree
   *       you operate on.
   */
  static already_AddRefed<nsIDOMCSSStyleDeclaration>
    GetComputedStyleDeclaration(nsIContent* aContent);

  /**
   * Start window emulation if presence of specific AT is detected.
   */
  static bool MaybeStartWindowEmulation();

  /**
   * Free resources used for window emulation.
   */
  static void ShutdownWindowEmulation();

  /**
   * Return true if window emulation is started.
   */
  static bool IsWindowEmulationStarted();

  /**
   * Helper to register window class.
   */
  static void RegisterNativeWindow(LPCWSTR aWindowClass);

  /**
   * Helper to create a window.
   */
  static HWND CreateNativeWindow(LPCWSTR aWindowClass, HWND aParentWnd,
                                 int aX, int aY, int aWidth, int aHeight,
                                 bool aIsActive);

  /**
   * Helper to show window.
   */
  static void ShowNativeWindow(HWND aWnd);

  /**
   * Helper to hide window.
   */
  static void HideNativeWindow(HWND aWnd);

  /**
   * Cache for HWNDs of windows created for document accessibles in windows
   * emulation mode.
   */
  static nsRefPtrHashtable<nsPtrHashKey<void>, DocAccessible>* sHWNDCache;
};

} // namespace a11y
} // namespace mozilla

#endif
