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
#include "EmbedWindow.h"

#include "qgeckoembed.h"

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>
#include "nsIWidget.h"
#include "nsIWebNavigation.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"

#include <qapplication.h>
#include <qeventloop.h>
#include <qvbox.h>
#include <qwidget.h>
#include <qtooltip.h>
#include <qcursor.h>
#include <qlabel.h>

class MozTipLabel : public QLabel
{
public:
    MozTipLabel( QWidget* parent)
        : QLabel( parent, "toolTipTip",
                  Qt::WStyle_StaysOnTop | Qt::WStyle_Customize | Qt::WStyle_NoBorder
                  | Qt::WStyle_Tool | Qt::WX11BypassWM )
    {
        setMargin(1);
        setAutoMask( FALSE );
        setFrameStyle( QFrame::Plain | QFrame::Box );
        setLineWidth( 1 );
        setAlignment( AlignAuto | AlignTop );
        setIndent(0);
        polish();
        adjustSize();
        setFont(QToolTip::font());
        setPalette(QToolTip::palette());
    }
};


EmbedWindow::EmbedWindow()
    : mOwner(nsnull),
      mVisibility(PR_FALSE),
      mIsModal(PR_FALSE),
      tooltip(0)
{
}

EmbedWindow::~EmbedWindow(void)
{
    ExitModalEventLoop(PR_FALSE);
    if (tooltip)
        delete tooltip;
}

void
EmbedWindow::Init(QGeckoEmbed *aOwner)
{
    // save our owner for later
    mOwner = aOwner;

    // create our nsIWebBrowser object and set up some basic defaults.
    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    if (!mWebBrowser) {
        //log an error
        return;
    }

    mWebBrowser->SetContainerWindow(NS_STATIC_CAST(nsIWebBrowserChrome *, this));

    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(mWebBrowser);
    item->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

}

nsresult
EmbedWindow::CreateWindow(void)
{
    nsresult rv;

    // Get the base window interface for the web browser object and
    // create the window.
    mBaseWindow = do_QueryInterface(mWebBrowser);
    rv = mBaseWindow->InitWindow(mOwner,
                                 nsnull,
                                 0, 0,
                                 mOwner->width(),
                                 mOwner->height());
    if (NS_FAILED(rv))
        return rv;

    rv = mBaseWindow->Create();
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

void
EmbedWindow::ReleaseChildren(void)
{
    ExitModalEventLoop(PR_FALSE);

    mBaseWindow->Destroy();
    mBaseWindow = 0;
    mWebBrowser = 0;
}

// nsISupports

NS_IMPL_ADDREF(EmbedWindow)
    NS_IMPL_RELEASE(EmbedWindow)

    NS_INTERFACE_MAP_BEGIN(EmbedWindow)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
    NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
    NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
    NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_END

// nsIWebBrowserChrome

NS_IMETHODIMP
EmbedWindow::SetStatus(PRUint32 aStatusType, const PRUnichar *aStatus)
{
    switch (aStatusType) {
    case STATUS_SCRIPT:
    {
        mOwner->emitScriptStatus(QString::fromUcs2(aStatus));
    }
    break;
    case STATUS_SCRIPT_DEFAULT:
        // Gee, that's nice.
        break;
    case STATUS_LINK:
    {
        mLinkMessage = aStatus;
        mOwner->emitLinkStatus(QString::fromUcs2(aStatus));
    }
    break;
    }
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetWebBrowser(nsIWebBrowser **aWebBrowser)
{
    *aWebBrowser = mWebBrowser;
    NS_IF_ADDREF(*aWebBrowser);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetWebBrowser(nsIWebBrowser *aWebBrowser)
{
    mWebBrowser = aWebBrowser;
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetChromeFlags(PRUint32 *aChromeFlags)
{
    *aChromeFlags = mOwner->chromeMask();
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetChromeFlags(PRUint32 aChromeFlags)
{
    mOwner->setChromeMask(aChromeFlags);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::DestroyBrowserWindow(void)
{
    emit mOwner->destroyBrowser();

    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
    emit mOwner->sizeTo(aCX, aCY);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ShowAsModal(void)
{
    qDebug("setting modal");
    mIsModal = PR_TRUE;
    qApp->eventLoop()->enterLoop();
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::IsWindowModal(PRBool *_retval)
{
    *_retval = mIsModal;
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::ExitModalEventLoop(nsresult aStatus)
{
    qDebug("exiting modal");
    qApp->eventLoop()->exitLoop();
    return NS_OK;
}

// nsIWebBrowserChromeFocus

NS_IMETHODIMP
EmbedWindow::FocusNextElement()
{
    //FIXME:
    //i think gecko handles that internally
    //mOwner->focusNextPrevChild(TRUE);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::FocusPrevElement()
{
    //FIXME: same story as above
    //mOwner->focusNextPrevChild(FALSE);
    return NS_OK;
}

// nsIEmbeddingSiteWindow

NS_IMETHODIMP
EmbedWindow::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY,
                           PRInt32 aCX, PRInt32 aCY)
{
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
        (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                   nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
        return mBaseWindow->SetPositionAndSize(aX, aY, aCX, aCY, PR_TRUE);
    }
    else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
        return mBaseWindow->SetPosition(aX, aY);
    }
    else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                       nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
        return mBaseWindow->SetSize(aCX, aCY, PR_TRUE);
    }
    return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
EmbedWindow::GetDimensions(PRUint32 aFlags, PRInt32 *aX,
                           PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY)
{
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
        (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                   nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER))) {
        return mBaseWindow->GetPositionAndSize(aX, aY, aCX, aCY);
    }
    else if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
        return mBaseWindow->GetPosition(aX, aY);
    }
    else if (aFlags & (nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER |
                       nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER)) {
        return mBaseWindow->GetSize(aCX, aCY);
    }
    return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
EmbedWindow::SetFocus(void)
{
    // XXX might have to do more here.
    return mBaseWindow->SetFocus();
}

NS_IMETHODIMP
EmbedWindow::GetTitle(PRUnichar **aTitle)
{
    *aTitle = ToNewUnicode(mTitle);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetTitle(const PRUnichar *aTitle)
{
    mTitle = aTitle;
    emit mOwner->windowTitleChanged(QString::fromUcs2(aTitle));
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetSiteWindow(void **aSiteWindow)
{
    *aSiteWindow = NS_STATIC_CAST(void *, mOwner);
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::GetVisibility(PRBool *aVisibility)
{
    *aVisibility = mVisibility;
    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::SetVisibility(PRBool aVisibility)
{
    // We always set the visibility so that if it's chrome and we finish
    // the load we know that we have to show the window.
    mVisibility = aVisibility;

    // if this is a chrome window and the chrome hasn't finished loading
    // yet then don't show the window yet.
    if (mOwner->isChrome() && !mOwner->chromeLoaded())
        return NS_OK;

    emit mOwner->visibilityChanged(aVisibility);

    return NS_OK;
}

// nsITooltipListener

NS_IMETHODIMP
EmbedWindow::OnShowTooltip(PRInt32 aXCoords, PRInt32 aYCoords, const PRUnichar *aTipText)
{
    QString tipText = QString::fromUcs2(aTipText);

    // get the root origin for this content window
    nsCOMPtr<nsIWidget> mainWidget;
    mBaseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    QWidget *window;
    window = NS_STATIC_CAST(QWidget*, mainWidget->GetNativeData(NS_NATIVE_WINDOW));

    if (!window) {
        NS_ERROR("no qt window in hierarchy!\n");
        return NS_ERROR_FAILURE;
    }

    int screen = qApp->desktop()->screenNumber(window);
    if (!tooltip || qApp->desktop()->screenNumber(tooltip) != screen) {
        delete tooltip;
        QWidget *s = QApplication::desktop()->screen(screen);
        tooltip = new MozTipLabel(s);
    }

    tooltip->setText(tipText);
    tooltip->resize(tooltip->sizeHint());
    QPoint pos(aXCoords, aYCoords+24);
    pos = window->mapToGlobal(pos);
    tooltip->move(pos);
    tooltip->show();

    return NS_OK;
}

NS_IMETHODIMP
EmbedWindow::OnHideTooltip(void)
{
    if (tooltip)
        tooltip->hide();
    return NS_OK;
}


NS_IMETHODIMP
EmbedWindow::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{
//     if (!aEvent->type == NS_CONTEXTMENU)
//         return NS_OK;
    qDebug("EmbedWindow::OnShowContextMenu");

    QString url = mOwner->url();

    PRUint16 nodeType;
    aNode->GetNodeType(&nodeType);
    if (nodeType == nsIDOMNode::ELEMENT_NODE) {
        nsIDOMElement *element = static_cast<nsIDOMElement *>(aNode);
        nsString tagname;
        element->GetTagName(tagname);
        nsCString ctagname;
        LossyCopyUTF16toASCII(tagname, ctagname);
        if (!strcasecmp(ctagname.get(), "a")) {
            nsString href;
            nsString attr;
            attr.AssignLiteral("href");
            element->GetAttribute(attr, href);
            url = mOwner->resolvedUrl(QString::fromUcs2(href.get()));
        } else if (!strcasecmp(ctagname.get(), "img")) {
            nsString href;
            nsString attr;
            attr.AssignLiteral("src");
            element->GetAttribute(attr, href);
            url = mOwner->resolvedUrl(QString::fromUcs2(href.get()));
        }
    }

    emit mOwner->showContextMenu(QCursor::pos(), url);
    return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
EmbedWindow::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
    nsresult rv;

    rv = QueryInterface(aIID, aInstancePtr);

    // pass it up to the web browser object
    if (NS_FAILED(rv) || !*aInstancePtr) {
        nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(mWebBrowser);
        return ir->GetInterface(aIID, aInstancePtr);
    }

    return rv;
}
