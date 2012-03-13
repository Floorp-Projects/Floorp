/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef nsWinUtils_h_
#define nsWinUtils_h_

#include "Accessible2.h"

#include "nsIArray.h"
#include "nsIDocument.h"

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
   * Convert nsIArray array of accessible objects to an array of IUnknown*
   * objects used in IA2 methods.
   */
  static HRESULT ConvertToIA2Array(nsIArray *aCollection,
                                   IUnknown ***aAccessibles, long *aCount);

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
};

#endif

