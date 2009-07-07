/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */

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

using namespace mozilla::tabs;

TabChild::TabChild()
    : mWidget(0)
{
}

TabChild::~TabChild()
{
    // TODObsmedberg: destroy the window!
}

bool
TabChild::Init(MessageLoop* aIOLoop, IPC::Channel* aChannel)
{
    Open(aChannel, aIOLoop);
    return true;
}

nsresult
TabChild::Answerinit(const MagicWindowHandle& parentWidget)
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
    
    // TODObz: create and embed a window!
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
TabChild::AnswerloadURL(const String& uri)
{
    printf("loading %s, %d\n", uri.c_str(), NS_IsMainThread());

    return mWebNav->LoadURI(NS_ConvertUTF8toUTF16(uri.c_str()).get(),
                            nsIWebNavigation::LOAD_FLAGS_NONE,
                            NULL, NULL, NULL); 
}

nsresult
TabChild::Answermove(const PRUint32& x,
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
