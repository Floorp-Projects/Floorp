/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com) (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHTMLWin32ObjectAccessible.h"
#include "nsAccessibleWrap.h"

nsHTMLWin32ObjectAccessible::nsHTMLWin32ObjectAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell, void* aHwnd):
nsAccessibleWrap(aNode, aShell)
{
  if (aHwnd) {
    // XXX - when we get accessible plugins we may have to check here
    //       for the proper window handle using Win32 APIs so we check
    //       the proper IAccessible for the information we need.
    mHwnd = aHwnd;
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLWin32ObjectAccessible, nsAccessible, nsIAccessibleWin32Object)

NS_IMETHODIMP
nsHTMLWin32ObjectAccessible::GetHwnd(void **aHwnd) 
{
  *aHwnd = mHwnd;
  if (mHwnd) {
    // Check for NullPluginClass to avoid infinite loop bug 245878
    // XXX We're going to be doing more work with NullPluginClass at some point to make it
    // keyboard accessible (bug 245349, bug 83754 as well as other work going on in
    // plugins to make them accessible like bug 93149).
    // The eventual goal will be to get rid of this check here so we 
    // can walk into the null plugin.
    const LPCSTR kClassNameNullPlugin = "NullPluginClass";
    HWND nullPluginChild = ::FindWindowEx((HWND)mHwnd, NULL, kClassNameNullPlugin, NULL);
    if (nullPluginChild) {
      *aHwnd = 0;    // Avoid infinite loop by not walking into null plugin
    }
  }
  return NS_OK;
}

