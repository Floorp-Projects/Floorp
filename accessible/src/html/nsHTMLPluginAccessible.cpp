/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *          John Gaunt (jgaunt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLPluginAccessible.h"
#include "nsIContent.h"
#include "nsObjectFrame.h"
#include "nsplugindefs.h"
#include "nsAccessibilityService.h"

nsHTMLPluginAccessible::nsHTMLPluginAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell), mAccService(do_GetService("@mozilla.org/accessibilityService;1"))
{
}

NS_IMETHODIMP
nsHTMLPluginAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
    return NS_ERROR_FAILURE;

  nsObjectFrame* objectFrame = NS_STATIC_CAST(nsObjectFrame*, frame);
#ifdef XP_WIN
  HWND pluginPort = nsnull;
  objectFrame->GetPluginPort(&pluginPort);
  if (pluginPort) {
    if (mAccService)
      mAccService->CreateHTMLNativeWindowAccessible(mDOMNode, mPresShell, (PRInt32)pluginPort, _retval);
  }
#else
  *_retval = nsnull;
#endif
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  return GetAccFirstChild(_retval);
}

NS_IMETHODIMP
nsHTMLPluginAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLPluginAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_WINDOW;
  return NS_OK;
}

