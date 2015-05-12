/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellCID_h__
#define nsDocShellCID_h__

#define NS_GLOBALHISTORY2_CONTRACTID "@mozilla.org/browser/global-history;2"

/**
 * A contract for a service that will track download history.  This can be
 * overridden by embedders if they would like to track additional information
 * about downloads.
 *
 * @implements nsIDownloadHistory
 */
#define NS_DOWNLOADHISTORY_CONTRACTID "@mozilla.org/browser/download-history;1"

/**
 * A contract that can be used to get a service that provides
 * meta-information about nsIWebNavigation objects' capabilities.
 * @implements nsIWebNavigationInfo
 */
#define NS_WEBNAVIGATION_INFO_CONTRACTID "@mozilla.org/webnavigation-info;1"

/**
 * Class and contract ID for the docshell.  This is the container for a web
 * navigation context.  It implements too many interfaces to count, and the
 * exact ones keep changing; if they stabilize somewhat that will get
 * documented.
 */
#define NS_DOCSHELL_CID                                                        \
  { 0xf1eac762, 0x87e9, 0x11d3,                                                \
    { 0xaf, 0x80, 0x00, 0xa0, 0x24, 0xff, 0xc0, 0x8c } }
#define NS_DOCSHELL_CONTRACTID "@mozilla.org/docshell/html;1"

/**
 * Contract ID to obtain the IHistory interface.  This is a non-scriptable
 * interface used to interact with history in an asynchronous manner.
 */
#define NS_IHISTORY_CONTRACTID "@mozilla.org/browser/history;1"

/**
 * A contract for a service that is used for finding
 * platform-specific applications for handling particular URLs.
 *
 * @implements nsIExternalURLHandlerService
 */
#define NS_EXTERNALURLHANDLERSERVICE_CONTRACTID \
  "@mozilla.org/uriloader/external-url-handler-service;1"

/**
 * An observer service topic that can be listened to to catch creation
 * of content browsing areas (both toplevel ones and subframes).  The
 * subject of the notification will be the nsIWebNavigation being
 * created.  At this time the additional data wstring is not defined
 * to be anything in particular.
 */
#define NS_WEBNAVIGATION_CREATE "webnavigation-create"

/**
 * An observer service topic that can be listened to to catch creation
 * of chrome browsing areas (both toplevel ones and subframes).  The
 * subject of the notification will be the nsIWebNavigation being
 * created.  At this time the additional data wstring is not defined
 * to be anything in particular.
 */
#define NS_CHROME_WEBNAVIGATION_CREATE "chrome-webnavigation-create"

/**
 * An observer service topic that can be listened to to catch destruction
 * of content browsing areas (both toplevel ones and subframes).  The
 * subject of the notification will be the nsIWebNavigation being
 * destroyed.  At this time the additional data wstring is not defined
 * to be anything in particular.
 */
#define NS_WEBNAVIGATION_DESTROY "webnavigation-destroy"

/**
 * An observer service topic that can be listened to to catch destruction
 * of chrome browsing areas (both toplevel ones and subframes).  The
 * subject of the notification will be the nsIWebNavigation being
 * destroyed.  At this time the additional data wstring is not defined
 * to be anything in particular.
 */
#define NS_CHROME_WEBNAVIGATION_DESTROY "chrome-webnavigation-destroy"

#endif // nsDocShellCID_h__
