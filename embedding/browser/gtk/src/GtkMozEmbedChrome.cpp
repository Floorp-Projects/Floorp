/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "GtkMozEmbedChrome.h"

// this is a define to make sure that we don't call certain function
// before the object has been properly initialized

#define CHECK_IS_INIT()                                \
        PR_BEGIN_MACRO                                 \
          if (!mIsInitialized)                         \
            return NS_ERROR_NOT_INITIALIZED;           \
        PR_END_MACRO

// constructor and destructor
GtkMozEmbedChrome::GtkMozEmbedChrome()
{
  NS_INIT_REFCNT();
}

GtkMozEmbedChrome::~GtkMozEmbedChrome()
{
}

// nsISupports interface

NS_IMPL_ADDREF(GtkMozEmbedChrome)
NS_IMPL_RELEASE(GtkMozEmbedChrome)

NS_INTERFACE_MAP_BEGIN(GtkMozEmbedChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGtkEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIGtkEmbed)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

// nsIGtkEmbed interface

NS_IMETHODIMP GtkMozEmbedChrome::Init(GdkWindow *aParentWindow)
{
  g_print("GtkMozEmbedChrome::Init\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIInterfaceRequestor interface

NS_IMETHODIMP GtkMozEmbedChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
  g_print("GtkMozEmbedChrome::GetInterface\n");
  return QueryInterface(aIID, aInstancePtr);
}

// nsIWebBrowserChrome interface

NS_IMETHODIMP GtkMozEmbedChrome::SetJSStatus(const PRUnichar *status)
{
  g_print("GtkMozEmbedChrome::SetJSStatus\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetJSDefaultStatus(const PRUnichar *status)
{
  g_print("GtkMozEmbedChrome::SetJSDefaultStatus\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetOverLink(const PRUnichar *link)
{
  g_print("GtkMozEmbedChrome::SetOverLink\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
  g_print("GtkMozEmbedChrome::GetWebBrowser\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
  g_print("GtkMozEmbedChrome::SetWebBrowser\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetChromeMask(PRUint32 *aChromeMask)
{
  g_print("GtkMozEmbedChrome::GetChromeMask\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetChromeMask(PRUint32 aChromeMask)
{
  g_print("GtkMozEmbedChrome::SetChromeMask\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetNewBrowser(PRUint32 chromeMask, 
					       nsIWebBrowser **_retval)
{
  g_print("GtkMozEmbedChrome::GetNewBrowser\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::FindNamedBrowserItem(const PRUnichar *aName, 
						      nsIDocShellTreeItem **_retval)
{
  g_print("GtkMozEmbedChrome::FindNamedBrowser\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  g_print("GtkMozEmbedChrome::SizeBrowserTo\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::ShowAsModal(void)
{
  g_print("GtkMozEmbedChrome::ShowAsModal\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsIBaseWindow interface

NS_IMETHODIMP GtkMozEmbedChrome::InitWindow(nativeWindow parentNativeWindow,
					    nsIWidget * parentWidget, 
					    PRInt32 x, PRInt32 y,
					    PRInt32 cx, PRInt32 cy)
{
  g_print("GtkMozEmbedChrome::InitWindow\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::Create(void)
{
  g_print("GtkMozEmbedChrome::Create\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::Destroy(void)
{
  g_print("GtkMozEmbedChrome::Destory\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetPosition(PRInt32 x, PRInt32 y)
{
  g_print("GtkMozEmbedChrome::SetPosition\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetPosition(PRInt32 *x, PRInt32 *y)
{
  g_print("GtkMozEmbedChrome::GetPosition\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
  g_print("GtkMozEmbedChrome::SetSize\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetSize(PRInt32 *cx, PRInt32 *cy)
{
  g_print("GtkMozEmbedChrome::GetSize\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetPositionAndSize(PRInt32 x, PRInt32 y,
						    PRInt32 cx, PRInt32 cy,
						    PRBool fRepaint)
{
  g_print("GtkMozEmbedChrome::SetPositionAndSize\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetPositionAndSize(PRInt32 *x, PRInt32 *y,
						    PRInt32 *cx, PRInt32 *cy)
{
  g_print("GtkMozEmbedChrome::GetPositionAndSize\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::Repaint(PRBool force)
{
  g_print("GtkMozEmbedCHrome::Repaint\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetParentWidget(nsIWidget * *aParentWidget)
{
  g_print("GtkMozEmbedChrome::GetParentWidget\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetParentWidget(nsIWidget * aParentWidget)
{
  g_print("GtkMozEmbedChrome::SetParentWidget\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetParentNativeWindow(nativeWindow *aParentNativeWindow)
{
  g_print("GtkMozEmbedChrome::GetParentNativeWindow\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP GtkMozEmbedChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  g_print("GtkMozEmbedChrome::SetParentNativeWindow\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetVisibility(PRBool *aVisibility)
{
  g_print("GtkMozEmbedChrome::GetVisibility\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetVisibility(PRBool aVisibility)
{
  g_print("GtkMozEmbedChrome::SetVisibility\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetMainWidget(nsIWidget * *aMainWidget)
{
  g_print("GtkMozEmbedChrome::GetMainWidget\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::SetFocus(void)
{
  g_print("GtkMozEmbedChrome::SetFocus\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::FocusAvailable(nsIBaseWindow *aCurrentFocus, 
						PRBool *aTookFocus)
{
  g_print("GtkMozEmbedChrome::FocusAvailable\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GtkMozEmbedChrome::GetTitle(PRUnichar * *aTitle)
{
  g_print("GtkMozEmbedChrome::GetTitle\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP GtkMozEmbedChrome::SetTitle(const PRUnichar * aTitle)
{
  g_print("GtkMozEmbedChrome::SetTitle\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}



