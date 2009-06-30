/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */

#include "TabChild.h"

#include "nsIWebBrowser.h"
#include "nsEmbedCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsThreadUtils.h"
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

using namespace mozilla::tabs;

TabChild::TabChild()
    : mWidget(0)
    , mChild(this)
{
}

TabChild::~TabChild()
{
    // TODObsmedberg: destroy the window!
}

bool
TabChild::Init(MessageLoop* aIOLoop, IPC::Channel* aChannel)
{
    mChild.Open(aChannel, aIOLoop);
    return true;
}

nsresult
TabChild::init(const MagicWindowHandle& parentWidget)
{
    printf("creating %d!\n", NS_IsMainThread());

    gtk_init(NULL, NULL);

    nsCOMPtr<nsIWebBrowser> webBrowser(do_CreateInstance(NS_WEBBROWSER_CONTRACTID));

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);

    GtkWidget* win = gtk_plug_new((GdkNativeWindow)parentWidget);
    gtk_widget_show(win);
    baseWindow->InitWindow(win, 0, 0, 0, 0, 0);

    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
    docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

    baseWindow->Create();

    baseWindow->SetVisibility(PR_TRUE);

    mWebNav = do_QueryInterface(webBrowser);
    
    // TODObz: create and embed a window!
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
TabChild::loadURL(const String& uri)
{
    printf("loading %s, %d\n", uri.c_str(), NS_IsMainThread());

    return mWebNav->LoadURI(NS_ConvertUTF8toUTF16(uri.c_str()).get(),
                            nsIWebNavigation::LOAD_FLAGS_NONE,
                            NULL, NULL, NULL); 
}

nsresult
TabChild::move(const uint32_t& x,
               const uint32_t& y,
               const uint32_t& width,
               const uint32_t& height)
{
    printf("[TabChild] MOVE to (x,y)=(%ud, %ud), (w,h)= (%ud, %ud)\n",
           x, y, width, height);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(x, y, width, height, PR_TRUE);
    return NS_OK;
}
