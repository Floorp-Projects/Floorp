/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=4 sts=2 tw=80 et cindent: */
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
 * Oleg Romashin. Portions created by Oleg Romashin are Copyright (C) Oleg Romashin.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include "EmbedGtkTools.h"
#ifndef MOZILLA_INTERNAL_API
#include "nsServiceManagerUtils.h"
#endif
#include "EmbedPrivate.h"

GtkWidget * GetGtkWidgetForDOMWindow(nsIDOMWindow* aDOMWindow)
{
  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");
  if (!aDOMWindow)
    return NULL;
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  wwatch->GetChromeForWindow(aDOMWindow, getter_AddRefs(chrome));
  if (!chrome) {
    return GTK_WIDGET(EmbedCommon::GetAnyLiveWidget());
  }

  nsCOMPtr<nsIEmbeddingSiteWindow> siteWindow = nsnull;
  siteWindow = do_QueryInterface(chrome);

  if (!siteWindow) {
    return GTK_WIDGET(EmbedCommon::GetAnyLiveWidget());
  }

  GtkWidget* parentWidget;
  siteWindow->GetSiteWindow((void**)&parentWidget);
  if (GTK_IS_WIDGET(parentWidget))
    return parentWidget;
  return NULL;
}

GtkWindow * GetGtkWindowForDOMWindow(nsIDOMWindow* aDOMWindow)
{
  GtkWidget* parentWidget = GetGtkWidgetForDOMWindow(aDOMWindow);
  if (!parentWidget)
    return NULL;
  GtkWidget* gtkWin = gtk_widget_get_toplevel(parentWidget);
  if (GTK_WIDGET_TOPLEVEL(gtkWin))
    return GTK_WINDOW(gtkWin);
  return NULL;
}

nsresult GetContentViewer(nsIWebBrowser *webBrowser, nsIContentViewer **aViewer)
{
  g_return_val_if_fail(webBrowser, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShell> docShell(do_GetInterface((nsISupports*)webBrowser));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  return docShell->GetContentViewer(aViewer);
}
