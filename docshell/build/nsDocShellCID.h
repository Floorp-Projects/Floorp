/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellCID_h__
#define nsDocShellCID_h__

/**
 * A contract that can be used to get a service that provides
 * meta-information about nsIWebNavigation objects' capabilities.
 * @implements nsIWebNavigationInfo
 */
#define NS_WEBNAVIGATION_INFO_CONTRACTID "@mozilla.org/webnavigation-info;1"

/**
 * Contract ID to obtain the IHistory interface.  This is a non-scriptable
 * interface used to interact with history in an asynchronous manner.
 */
#define NS_IHISTORY_CONTRACTID "@mozilla.org/browser/history;1"

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

#endif  // nsDocShellCID_h__
