/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "TabChild.h"

#include "nsIWebBrowser.h"
#include "nsEmbedCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

using namespace mozilla::dom;

TabChild::TabChild(const MagicWindowHandle& parentWidget)
{
    printf("creating %d!\n", NS_IsMainThread());

#ifdef MOZ_WIDGET_GTK2
    gtk_init(NULL, NULL);
#endif

    nsCOMPtr<nsIWebBrowser> webBrowser(do_CreateInstance(NS_WEBBROWSER_CONTRACTID));

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);


#ifdef MOZ_WIDGET_GTK2
    GtkWidget* win = gtk_plug_new((GdkNativeWindow)parentWidget);
    gtk_widget_show(win);
#elif defined(XP_WIN)
    HWND win = parentWidget;
#else
#error You lose!
#endif

    baseWindow->InitWindow(win, 0, 0, 0, 0, 0);

    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
    docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

    baseWindow->Create();

    baseWindow->SetVisibility(PR_TRUE);

    mWebNav = do_QueryInterface(webBrowser);
}

TabChild::~TabChild()
{
    // TODObsmedberg: destroy the window!
}

nsresult
TabChild::RecvloadURL(const nsCString& uri)
{
    printf("loading %s, %d\n", uri.get(), NS_IsMainThread());

    return mWebNav->LoadURI(NS_ConvertUTF8toUTF16(uri).get(),
                            nsIWebNavigation::LOAD_FLAGS_NONE,
                            NULL, NULL, NULL); 
}

nsresult
TabChild::Recvmove(const PRUint32& x,
                     const PRUint32& y,
                     const PRUint32& width,
                     const PRUint32& height)
{
    printf("[TabChild] MOVE to (x,y)=(%ud, %ud), (w,h)= (%ud, %ud)\n",
           x, y, width, height);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(x, y, width, height, PR_TRUE);
    return NS_OK;
}
