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
 *    Travis Bogard <travis@netscape.com> 
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

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsBarProps.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMWindowInternal.h"
#include "nsDOMClassInfo.h"

//
//  Basic (virtual) BarProp class implementation
//
BarPropImpl::BarPropImpl() : mBrowserChrome(nsnull)
{
  NS_INIT_REFCNT();
}

BarPropImpl::~BarPropImpl()
{
}


// QueryInterface implementation for BarPropImpl
NS_INTERFACE_MAP_BEGIN(BarPropImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBarProp)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BarProp)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(BarPropImpl)
NS_IMPL_RELEASE(BarPropImpl)


NS_IMETHODIMP
BarPropImpl::SetWebBrowserChrome(nsIWebBrowserChrome* aBrowserChrome)
{
  mBrowserChrome = aBrowserChrome;
  return NS_OK;
}

NS_IMETHODIMP
BarPropImpl::GetVisibleByFlag(PRBool *aVisible, PRUint32 aChromeFlag)
{
  NS_ENSURE_TRUE(mBrowserChrome, NS_ERROR_FAILURE);

  PRUint32 chromeFlags;
  *aVisible = PR_FALSE;

  NS_ENSURE_SUCCESS(mBrowserChrome->GetChromeFlags(&chromeFlags),
                    NS_ERROR_FAILURE);
  if(chromeFlags & aChromeFlag)
    *aVisible = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
BarPropImpl::SetVisibleByFlag(PRBool aVisible, PRUint32 aChromeFlag)
{
  NS_ENSURE_TRUE(mBrowserChrome, NS_ERROR_FAILURE);

  PRUint32 chromeFlags;

  NS_ENSURE_SUCCESS(mBrowserChrome->GetChromeFlags(&chromeFlags),
                    NS_ERROR_FAILURE);
  if(aVisible)
    chromeFlags |= aChromeFlag;
  else
    chromeFlags &= ~aChromeFlag;
  NS_ENSURE_SUCCESS(mBrowserChrome->SetChromeFlags(chromeFlags),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

//
// MenubarProp class implementation
//

MenubarPropImpl::MenubarPropImpl()
{
}

MenubarPropImpl::~MenubarPropImpl()
{
}

NS_IMETHODIMP
MenubarPropImpl::GetVisible(PRBool *aVisible)
{
  return BarPropImpl::GetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_MENUBAR);
}

NS_IMETHODIMP
MenubarPropImpl::SetVisible(PRBool aVisible)
{
  return BarPropImpl::SetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_MENUBAR);
}

//
// ToolbarProp class implementation
//

ToolbarPropImpl::ToolbarPropImpl()
{
}

ToolbarPropImpl::~ToolbarPropImpl()
{
}

NS_IMETHODIMP
ToolbarPropImpl::GetVisible(PRBool *aVisible)
{
  return BarPropImpl::GetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_TOOLBAR);
}

NS_IMETHODIMP
ToolbarPropImpl::SetVisible(PRBool aVisible) 
{
  return BarPropImpl::SetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_TOOLBAR);
}

//
// LocationbarProp class implementation
//

LocationbarPropImpl::LocationbarPropImpl()
{
}

LocationbarPropImpl::~LocationbarPropImpl() 
{
}

NS_IMETHODIMP
LocationbarPropImpl::GetVisible(PRBool *aVisible) 
{
  return BarPropImpl::GetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

NS_IMETHODIMP
LocationbarPropImpl::SetVisible(PRBool aVisible) 
{
  return BarPropImpl::SetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_LOCATIONBAR);
}

//
// PersonalbarProp class implementation
//

PersonalbarPropImpl::PersonalbarPropImpl() 
{
}

PersonalbarPropImpl::~PersonalbarPropImpl() 
{
}

NS_IMETHODIMP
PersonalbarPropImpl::GetVisible(PRBool *aVisible) 
{
  return BarPropImpl::GetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

NS_IMETHODIMP
PersonalbarPropImpl::SetVisible(PRBool aVisible) 
{
  return BarPropImpl::SetVisibleByFlag(aVisible, nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
}

//
// StatusbarProp class implementation
//

StatusbarPropImpl::StatusbarPropImpl() 
{
}

StatusbarPropImpl::~StatusbarPropImpl() 
{
}

NS_IMETHODIMP
StatusbarPropImpl::GetVisible(PRBool *aVisible) 
{
  return BarPropImpl::GetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_STATUSBAR);
}

NS_IMETHODIMP
StatusbarPropImpl::SetVisible(PRBool aVisible) 
{
  return BarPropImpl::SetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_STATUSBAR);
}

//
// ScrollbarsProp class implementation
//

ScrollbarsPropImpl::ScrollbarsPropImpl() 
{
}

ScrollbarsPropImpl::~ScrollbarsPropImpl() 
{
}

NS_IMETHODIMP
ScrollbarsPropImpl::GetVisible(PRBool *aVisible) 
{
  return BarPropImpl::GetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_SCROLLBARS);
}

NS_IMETHODIMP
ScrollbarsPropImpl::SetVisible(PRBool aVisible) 
{
  return BarPropImpl::SetVisibleByFlag(aVisible,
                                       nsIWebBrowserChrome::CHROME_SCROLLBARS);
}
