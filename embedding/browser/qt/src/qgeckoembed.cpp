/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#include "qgeckoembed.h"

#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedStream.h"
#include "EmbedEventListener.h"
#include "EmbedContentListener.h"
#include "EmbedWindowCreator.h"
#include "qgeckoglobals.h"

#include "nsIAppShell.h"
#include <nsIDocShell.h>
#include <nsIWebProgress.h>
#include <nsIWebNavigation.h>
#include <nsIWebBrowser.h>
#include <nsISHistory.h>
#include <nsIWebBrowserChrome.h>
#include "nsIWidget.h"
#include "nsCRT.h"
#include <nsIWindowWatcher.h>
#include <nsILocalFile.h>
#include <nsEmbedAPI.h>
#include <nsWidgetsCID.h>
#include <nsIDOMUIEvent.h>
#include <nsIInterfaceRequestor.h>
#include <nsIComponentManager.h>
#include <nsIFocusController.h>
#include <nsProfileDirServiceProvider.h>
#include <nsIGenericFactory.h>
#include <nsIComponentRegistrar.h>
#include <nsIPref.h>
#include <nsVoidArray.h>
#include <nsIDOMDocument.h>
#include <nsIDOMBarProp.h>
#include <nsIDOMWindow.h>
#include <nsIDOMEventReceiver.h>
#include <nsCOMPtr.h>
#include <nsPIDOMWindow.h>

#include "prenv.h"

#include <qlayout.h>

class QGeckoEmbedPrivate
{
public:
    QGeckoEmbedPrivate(QGeckoEmbed *qq);
    ~QGeckoEmbedPrivate();

    QGeckoEmbed *q;

    QWidget *mMainWidget;

    // all of the objects that we own
    EmbedWindow                   *window;
    nsCOMPtr<nsISupports>          windowGuard;
    EmbedProgress                 *progress;
    nsCOMPtr<nsISupports>          progressGuard;
    EmbedContentListener          *contentListener;
    nsCOMPtr<nsISupports>          contentListenerGuard;
    EmbedEventListener            *eventListener;
    nsCOMPtr<nsISupports>          eventListenerGuard;
    EmbedStream                   *stream;
    nsCOMPtr<nsISupports>          streamGuard;

    nsCOMPtr<nsIWebNavigation>     navigation;
    nsCOMPtr<nsISHistory>          sessionHistory;

    // our event receiver
    nsCOMPtr<nsIDOMEventReceiver>  eventReceiver;

    // chrome mask
    PRUint32                       chromeMask;

    bool isChrome;
    bool chromeLoaded;
    bool listenersAttached;

    void initGUI();
    void init();
    void ApplyChromeMask();
};


QGeckoEmbedPrivate::QGeckoEmbedPrivate(QGeckoEmbed *qq)
    : q(qq),
      mMainWidget(0),
      chromeMask(nsIWebBrowserChrome::CHROME_ALL),
      isChrome(FALSE),
      chromeLoaded(FALSE),
      listenersAttached(FALSE)
{
    initGUI();
    init();
}

QGeckoEmbedPrivate::~QGeckoEmbedPrivate()
{
    QGeckoGlobals::removeEngine(q);
    QGeckoGlobals::popStartup();
}

void
QGeckoEmbedPrivate::init()
{
    QGeckoGlobals::initializeGlobalObjects();
    QGeckoGlobals::pushStartup();
    QGeckoGlobals::addEngine(q);

    // Create our embed window, and create an owning reference to it and
    // initialize it.  It is assumed that this window will be destroyed
    // when we go out of scope.
    window = new EmbedWindow();
    windowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, window);
    window->Init(q);
    // Create our progress listener object, make an owning reference,
    // and initialize it.  It is assumed that this progress listener
    // will be destroyed when we go out of scope.
    progress = new EmbedProgress(q);
    progressGuard = NS_STATIC_CAST(nsIWebProgressListener *,
                                   progress);

    // Create our content listener object, initialize it and attach it.
    // It is assumed that this will be destroyed when we go out of
    // scope.
    contentListener = new EmbedContentListener(q);
    contentListenerGuard = NS_STATIC_CAST(nsISupports*,
                                          NS_STATIC_CAST(nsIURIContentListener*, contentListener));

    // Create our key listener object and initialize it.  It is assumed
    // that this will be destroyed before we go out of scope.
    eventListener = new EmbedEventListener(q);
    eventListenerGuard =
        NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(nsIDOMKeyListener *,
                                                     eventListener));

    // has the window creator service been set up?
    static int initialized = PR_FALSE;
    // Set up our window creator ( only once )
    if (!initialized) {
        // create our local object
        nsCOMPtr<nsIWindowCreator> windowCreator = new EmbedWindowCreator();

        // Attach it via the watcher service
        nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
        if (watcher)
            watcher->SetWindowCreator(windowCreator);
        initialized = PR_TRUE;
    }

    // Get the nsIWebBrowser object for our embedded window.
    nsCOMPtr<nsIWebBrowser> webBrowser;
    window->GetWebBrowser(getter_AddRefs(webBrowser));

    // get a handle on the navigation object
    navigation = do_QueryInterface(webBrowser);

    // Create our session history object and tell the navigation object
    // to use it.  We need to do this before we create the web browser
    // window.
    sessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
    navigation->SetSessionHistory(sessionHistory);

    // create the window
    window->CreateWindow();

    // bind the progress listener to the browser object
    nsCOMPtr<nsISupportsWeakReference> supportsWeak;
    supportsWeak = do_QueryInterface(progressGuard);
    nsCOMPtr<nsIWeakReference> weakRef;
    supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
    webBrowser->AddWebBrowserListener(weakRef,
                                      nsIWebProgressListener::GetIID());

    // set ourselves as the parent uri content listener
    webBrowser->SetParentURIContentListener(contentListener);

    // save the window id of the newly created window
    nsCOMPtr<nsIWidget> qtWidget;
    window->mBaseWindow->GetMainWidget(getter_AddRefs(qtWidget));
    // get the native drawing area
    mMainWidget = NS_STATIC_CAST(QWidget*, qtWidget->GetNativeData(NS_NATIVE_WINDOW));

    // Apply the current chrome mask
    ApplyChromeMask();
}

void
QGeckoEmbedPrivate::initGUI()
{
    QBoxLayout *l = new QHBoxLayout(q);
    l->setAutoAdd(TRUE);
}

void
QGeckoEmbedPrivate::ApplyChromeMask()
{
   if (window) {
      nsCOMPtr<nsIWebBrowser> webBrowser;
      window->GetWebBrowser(getter_AddRefs(webBrowser));

      nsCOMPtr<nsIDOMWindow> domWindow;
      webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
      if (domWindow) {
          nsCOMPtr<nsIDOMBarProp> scrollbars;
          domWindow->GetScrollbars(getter_AddRefs(scrollbars));
          if (scrollbars) {

              scrollbars->SetVisible(
                  chromeMask & nsIWebBrowserChrome::CHROME_SCROLLBARS ?
                  PR_TRUE : PR_FALSE);
          }
      }
   }
}




QGeckoEmbed::QGeckoEmbed(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    d = new QGeckoEmbedPrivate(this);
}

QGeckoEmbed::~QGeckoEmbed()
{
    delete d;
}


bool
QGeckoEmbed::canGoBack() const
{
    PRBool retval = PR_FALSE;
    if (d->navigation)
        d->navigation->GetCanGoBack(&retval);
    return retval;
}

bool
QGeckoEmbed::canGoForward() const
{
    PRBool retval = PR_FALSE;
    if (d->navigation)
        d->navigation->GetCanGoForward(&retval);
    return retval;
}

void
QGeckoEmbed::loadURL(const QString &url)
{
    if (!url.isEmpty()) {
        d->navigation->LoadURI((const PRUnichar*)url.ucs2(),
                               nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                               nsnull,                            // Referring URI
                               nsnull,                            // Post data
                               nsnull);
    }
}

void
QGeckoEmbed::stopLoad()
{
    if (d->navigation)
        d->navigation->Stop(nsIWebNavigation::STOP_NETWORK);
}

void
QGeckoEmbed::goForward()
{
    if (d->navigation)
        d->navigation->GoForward();
}

void
QGeckoEmbed::goBack()
{
    if (d->navigation)
        d->navigation->GoBack();
}

void
QGeckoEmbed::renderData(const QCString &data, const QString &baseURI,
                            const QString &mimeType)
{
    openStream(baseURI, mimeType);
    appendData(data);
    closeStream();
}

int
QGeckoEmbed::openStream(const QString &baseURI, const QString &mimeType)
{
    nsresult rv;

    if (!d->stream) {
        d->stream = new EmbedStream();
        d->streamGuard = do_QueryInterface(d->stream);
        d->stream->InitOwner(this);
        rv = d->stream->Init();
        if (NS_FAILED(rv))
            return rv;
    }

    rv = d->stream->OpenStream(baseURI, mimeType);
    return rv;
}

int
QGeckoEmbed::appendData(const QCString &data)
{
    if (!d->stream)
        return NS_ERROR_FAILURE;

    // Attach listeners to this document since in some cases we don't
    // get updates for content added this way.
    contentStateChanged();

    return d->stream->AppendToStream(data, data.length());
}

int
QGeckoEmbed::closeStream()
{
    nsresult rv;

    if (!d->stream)
        return NS_ERROR_FAILURE;
    rv = d->stream->CloseStream();

    // release
    d->stream = 0;
    d->streamGuard = 0;

    return rv;
}

void
QGeckoEmbed::reload(ReloadFlags flags)
{
    int qeckoFlags = 0;
    switch(flags) {
    case Normal:
        qeckoFlags = 0;
        break;
    case BypassCache:
        qeckoFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
        break;
    case BypassProxy:
        qeckoFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
        break;
    case BypassProxyAndCache:
        qeckoFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                     nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
        break;
    case CharsetChange:
        qeckoFlags = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;
        break;
    default:
        qeckoFlags = 0;
        break;
    }


    nsCOMPtr<nsIWebNavigation> wn;

    if (d->sessionHistory) {
        wn = do_QueryInterface(d->sessionHistory);
    }
    if (!wn)
        wn = d->navigation;

    if (wn)
        wn->Reload(qeckoFlags);
}

bool
QGeckoEmbed::domKeyDownEvent(nsIDOMKeyEvent *keyEvent)
{
    qDebug("key event start");
    emit domKeyDown(keyEvent);
    qDebug("key event stop");
    return false;
}

bool
QGeckoEmbed::domKeyPressEvent(nsIDOMKeyEvent *keyEvent)
{
    emit domKeyPress(keyEvent);
    return false;
}

bool
QGeckoEmbed::domKeyUpEvent(nsIDOMKeyEvent *keyEvent)
{
    emit domKeyUp(keyEvent);
    qDebug("key release event stop");
    return false;
}

bool
QGeckoEmbed::domMouseDownEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseDown(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domMouseUpEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseUp(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domMouseClickEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseClick(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domMouseDblClickEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseDblClick(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domMouseOverEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseOver(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domMouseOutEvent(nsIDOMMouseEvent *mouseEvent)
{
    emit domMouseOut(mouseEvent);
    return false;
}

bool
QGeckoEmbed::domActivateEvent(nsIDOMUIEvent *event)
{
    emit domActivate(event);
    return false;
}

bool
QGeckoEmbed::domFocusInEvent(nsIDOMUIEvent *event)
{
    emit domFocusIn(event);
    return false;
}

bool
QGeckoEmbed::domFocusOutEvent(nsIDOMUIEvent *event)
{
    emit domFocusOut(event);
    return false;
}

void
QGeckoEmbed::emitScriptStatus(const QString &str)
{
    emit jsStatusMessage(str);
}

void
QGeckoEmbed::emitLinkStatus(const QString &str)
{
    emit linkMessage(str);
}

int
QGeckoEmbed::chromeMask() const
{
    return d->chromeMask;
}

void
QGeckoEmbed::setChromeMask(int mask)
{
    d->chromeMask = mask;

    d->ApplyChromeMask();
}

void
QGeckoEmbed::resizeEvent(QResizeEvent *e)
{
    d->window->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER,
                              0, 0, e->size().width(), e->size().height());
}

nsIDOMDocument*
QGeckoEmbed::document() const
{
    nsIDOMDocument *doc = 0;

    nsCOMPtr<nsIDOMWindow> window;
    nsCOMPtr<nsIWebBrowser> webBrowser;

    d->window->GetWebBrowser(getter_AddRefs(webBrowser));

    webBrowser->GetContentDOMWindow(getter_AddRefs(window));
    if (window) {
        window->GetDocument(&doc);
    }

    return doc;
}

void
QGeckoEmbed::contentStateChanged()
{
    // we don't attach listeners to chrome
    if (d->listenersAttached && !d->isChrome)
        return;

    setupListener();

    if (!d->eventReceiver)
        return;

    attachListeners();
}

void
QGeckoEmbed::contentFinishedLoading()
{
    if (d->isChrome) {
        // We're done loading.
        d->chromeLoaded = PR_TRUE;

        // get the web browser
        nsCOMPtr<nsIWebBrowser> webBrowser;
        d->window->GetWebBrowser(getter_AddRefs(webBrowser));

        // get the content DOM window for that web browser
        nsCOMPtr<nsIDOMWindow> domWindow;
        webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
        if (!domWindow) {
            NS_WARNING("no dom window in content finished loading\n");
            return;
        }

        // resize the content
        domWindow->SizeToContent();

        // and since we're done loading show the window, assuming that the
        // visibility flag has been set.
        PRBool visibility;
        d->window->GetVisibility(&visibility);
        if (visibility)
            d->window->SetVisibility(PR_TRUE);
    }
}

void
QGeckoEmbed::setupListener()
{
    if (d->eventReceiver)
        return;

    nsCOMPtr<nsPIDOMWindow> piWin;
    GetPIDOMWindow(getter_AddRefs(piWin));

    if (!piWin)
        return;

    d->eventReceiver = do_QueryInterface(piWin->GetChromeEventHandler());
}

void
QGeckoEmbed::attachListeners()
{
    if (!d->eventReceiver || d->listenersAttached)
        return;

    nsIDOMEventListener *eventListener =
        NS_STATIC_CAST(nsIDOMEventListener *,
                       NS_STATIC_CAST(nsIDOMKeyListener *, d->eventListener));

    // add the key listener
    nsresult rv;
    rv = d->eventReceiver->AddEventListenerByIID(eventListener,
                                                 NS_GET_IID(nsIDOMKeyListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add key listener\n");
        return;
    }

    rv = d->eventReceiver->AddEventListenerByIID(eventListener,
                                                 NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add mouse listener\n");
        return;
    }

    rv = d->eventReceiver->AddEventListenerByIID(eventListener,
                                                NS_GET_IID(nsIDOMUIListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add UI listener\n");
        return;
    }

    // ok, all set.
    d->listenersAttached = PR_TRUE;
}

EmbedWindow * QGeckoEmbed::window() const
{
    return d->window;
}


int QGeckoEmbed::GetPIDOMWindow(nsPIDOMWindow **aPIWin)
{
    *aPIWin = nsnull;

    // get the web browser
    nsCOMPtr<nsIWebBrowser> webBrowser;
    d->window->GetWebBrowser(getter_AddRefs(webBrowser));

    // get the content DOM window for that web browser
    nsCOMPtr<nsIDOMWindow> domWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        return NS_ERROR_FAILURE;

    // get the private DOM window
    nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
    // and the root window for that DOM window
    *aPIWin = domWindowPrivate->GetPrivateRoot();

    if (*aPIWin) {
        NS_ADDREF(*aPIWin);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;

}

void QGeckoEmbed::setIsChrome(bool isChrome)
{
    d->isChrome = isChrome;
}

bool QGeckoEmbed::isChrome() const
{
    return d->isChrome;
}

bool QGeckoEmbed::chromeLoaded() const
{
    return d->chromeLoaded;
}

QString QGeckoEmbed::url() const
{
    nsCOMPtr<nsIURI> uri;
    d->navigation->GetCurrentURI(getter_AddRefs(uri));
    nsCAutoString acstring;
    uri->GetSpec(acstring);

    return QString::fromUtf8(acstring.get());
}

QString QGeckoEmbed::resolvedUrl(const QString &relativepath) const
{
    nsCOMPtr<nsIURI> uri;
    d->navigation->GetCurrentURI(getter_AddRefs(uri));
    nsCAutoString rel;
    rel.Assign(relativepath.utf8().data());
    nsCAutoString resolved;
    uri->Resolve(rel, resolved);

    return QString::fromUtf8(resolved.get());
}

void QGeckoEmbed::initialize(const char *aDir, const char *aName)
{
    QGeckoGlobals::setProfilePath(aDir, aName);
}

