/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWinUtils_h_
#define nsWinUtils_h_

#include <functional>
#include <windows.h>

#include "nsICSSDeclaration.h"
#include "nsCOMPtr.h"

class nsIContent;

namespace mozilla {
namespace a11y {

class DocAccessible;

const LPCWSTR kClassNameRoot = L"MozillaUIWindowClass";
const LPCWSTR kClassNameTabContent = L"MozillaContentWindowClass";
const LPCWSTR kPropNameDocAcc = L"MozDocAccessible";
const LPCWSTR kPropNameDocAccParent = L"MozDocAccessibleParent";

class nsWinUtils
{
public:
  /**
   * Return computed styles declaration for the given node.
   *
   * @note Please use it carefully since it can shutdown the accessible tree
   *       you operate on.
   */
  static already_AddRefed<nsICSSDeclaration>
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
  static bool IsWindowEmulationStarted() { return sWindowEmulationStarted; }

  /**
   * Helper to register window class.
   */
  static void RegisterNativeWindow(LPCWSTR aWindowClass);

  typedef std::function<void(HWND)> NativeWindowCreateProc;

  /**
   * Helper to create a window.
   *
   * NB: If additional setup needs to be done once the window has been created,
   *     you should do so via aOnCreateProc. Hooks will fire during the
   *     CreateNativeWindow call, thus triggering events in the AT.
   *     Using aOnCreateProc guarantees that your additional initialization will
   *     have completed prior to the AT receiving window creation events.
   *
   *     For example:
   *
   *     nsWinUtils::NativeWindowCreateProc onCreate([](HWND aHwnd) -> void {
   *       DoSomeAwesomeInitializationStuff(aHwnd);
   *       DoMoreAwesomeInitializationStuff(aHwnd);
   *     });
   *     HWND hwnd = nsWinUtils::CreateNativeWindow(..., &onCreate);
   *     // Doing further initialization work to hwnd on this line is too late!
   */
  static HWND CreateNativeWindow(LPCWSTR aWindowClass, HWND aParentWnd,
                                 int aX, int aY, int aWidth, int aHeight,
                                 bool aIsActive,
                                 NativeWindowCreateProc* aOnCreateProc = nullptr);

  /**
   * Helper to show window.
   */
  static void ShowNativeWindow(HWND aWnd);

  /**
   * Helper to hide window.
   */
  static void HideNativeWindow(HWND aWnd);

private:
  /**
   * Flag that indicates if window emulation is started.
   */
  static bool sWindowEmulationStarted;
};

} // namespace a11y
} // namespace mozilla

#endif
