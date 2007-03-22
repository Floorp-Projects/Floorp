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
#ifndef EMBEDWINDOW_H
#define EMBEDWINDOW_H

#include <nsString.h>
#include <nsIWebBrowserChrome.h>
#include <nsIWebBrowserChromeFocus.h>
#include <nsIEmbeddingSiteWindow.h>
#include <nsITooltipListener.h>
#include <nsIContextMenuListener.h>
#include <nsISupports.h>
#include <nsIWebBrowser.h>
#include <nsIBaseWindow.h>
#include <nsIInterfaceRequestor.h>
#include <nsCOMPtr.h>
#include "nsString.h"

class QGeckoEmbed;
class MozTipLabel;

class EmbedWindow : public nsIWebBrowserChrome,
                    public nsIWebBrowserChromeFocus,
                    public nsIEmbeddingSiteWindow,
                    public nsITooltipListener,
                    public nsIContextMenuListener,
                    public nsIInterfaceRequestor
{
public:
    EmbedWindow();
    ~EmbedWindow();
    void Init(QGeckoEmbed *aOwner);

    nsresult CreateWindow    (void);
    void     ReleaseChildren (void);

    NS_DECL_ISUPPORTS

    NS_DECL_NSIWEBBROWSERCHROME

    NS_DECL_NSIWEBBROWSERCHROMEFOCUS

    NS_DECL_NSIEMBEDDINGSITEWINDOW

    NS_DECL_NSITOOLTIPLISTENER

    NS_DECL_NSICONTEXTMENULISTENER

    NS_DECL_NSIINTERFACEREQUESTOR

    nsString                 mTitle;
    nsString                 mJSStatus;
    nsString                 mLinkMessage;

    nsCOMPtr<nsIBaseWindow>  mBaseWindow; // [OWNER]

private:
    QGeckoEmbed              *mOwner;
    nsCOMPtr<nsIWebBrowser>  mWebBrowser; // [OWNER]
    PRBool                   mVisibility;
    PRBool                   mIsModal;
    MozTipLabel *tooltip;
};

#endif
