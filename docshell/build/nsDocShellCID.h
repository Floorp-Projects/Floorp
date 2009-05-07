/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4
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
 * The Original Code is the Mozilla gecko engine.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef nsDocShellCID_h__
#define nsDocShellCID_h__

#define NS_GLOBALHISTORY2_CONTRACTID \
    "@mozilla.org/browser/global-history;2"

/**
 * A contract for a service that will track download history.  This can be
 * overridden by embedders if they would like to track additional information
 * about downloads.
 *
 * @implements nsIDownloadHistory
 */
#define NS_DOWNLOADHISTORY_CONTRACTID \
    "@mozilla.org/browser/download-history;1"

/**
 * A contract that can be used to get a service that provides
 * meta-information about nsIWebNavigation objects' capabilities.
 * @implements nsIWebNavigationInfo
 */
#define NS_WEBNAVIGATION_INFO_CONTRACTID \
    "@mozilla.org/webnavigation-info;1"

/**
 * Contract ID for a service implementing nsIURIClassifier that identifies
 * phishing and malware sites.
 */
#define NS_URICLASSIFIERSERVICE_CONTRACTID "@mozilla.org/uriclassifierservice"

/**
 * Class and contract ID for an nsIChannelClassifier implementation for
 * checking a channel load against the URI classifier service.
 */
#define NS_CHANNELCLASSIFIER_CID \
 { 0xce02d538, 0x0217, 0x47a3,{0xa5, 0x89, 0xb5, 0x17, 0x90, 0xfd, 0xd8, 0xce}}

#define NS_CHANNELCLASSIFIER_CONTRACTID "@mozilla.org/channelclassifier"

/**
 * Class and contract ID for the docshell.  This is the container for a web
 * navigation context.  It implements too many interfaces to count, and the
 * exact ones keep changing; if they stabilize somewhat that will get
 * documented.
 */
#define NS_DOCSHELL_CID                                             \
    { 0xf1eac762, 0x87e9, 0x11d3,                                   \
      { 0xaf, 0x80, 0x00, 0xa0, 0x24, 0xff, 0xc0, 0x8c } }
#define NS_DOCSHELL_CONTRACTID "@mozilla.org/docshell/html;1"



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
