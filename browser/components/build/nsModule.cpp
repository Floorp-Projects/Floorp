/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#include "nsIGenericFactory.h"

#include "nsBrowserCompsCID.h"
#ifdef MOZ_PLACES
#include "nsAnnoProtocolHandler.h"
#include "nsAnnotationService.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"
#include "nsLivemarkService.h"
#include "nsMorkHistoryImporter.h"
#else
#include "nsBookmarksService.h"
#include "nsForwardProxyDataSource.h"
#endif
#ifdef XP_WIN
#include "nsWindowsShellService.h"
#elif defined(XP_MACOSX)
#include "nsMacShellService.h"
#elif defined(MOZ_WIDGET_GTK2)
#include "nsGNOMEShellService.h"
#endif
#include "nsProfileMigrator.h"
#if !defined(XP_BEOS)
#include "nsDogbertProfileMigrator.h"
#endif
#if !defined(XP_OS2)
#include "nsOperaProfileMigrator.h"
#endif
#include "nsPhoenixProfileMigrator.h"
#include "nsSeamonkeyProfileMigrator.h"
#if defined(XP_WIN) && !defined(__MINGW32__)
#include "nsIEProfileMigrator.h"
#elif defined(XP_MACOSX)
#include "nsSafariProfileMigrator.h"
#include "nsOmniWebProfileMigrator.h"
#include "nsMacIEProfileMigrator.h"
#include "nsCaminoProfileMigrator.h"
#include "nsICabProfileMigrator.h"
#endif
#include "rdf.h"
#ifdef MOZ_FEEDS
#include "nsFeedSniffer.h"
#include "nsAboutFeeds.h"
#include "nsIAboutModule.h"
#endif
#ifdef MOZ_SAFE_BROWSING
#include "nsDocNavStartProgressListener.h"
#endif

/////////////////////////////////////////////////////////////////////////////

#ifdef MOZ_PLACES
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNavHistory, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNavHistoryResultTreeViewer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAnnoProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAnnotationService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsNavBookmarks, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFaviconService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsLivemarkService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMorkHistoryImporter)
#else
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsBookmarksService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsForwardProxyDataSource, Init)
#endif
#ifdef XP_WIN
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindowsShellService)
#elif defined(XP_MACOSX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacShellService)
#elif defined(MOZ_WIDGET_GTK2)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGNOMEShellService, Init)
#endif
#if !defined(XP_BEOS)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDogbertProfileMigrator)
#endif
#if !defined(XP_OS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOperaProfileMigrator)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPhoenixProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSeamonkeyProfileMigrator)
#if defined(XP_WIN) && !defined(__MINGW32__)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIEProfileMigrator)
#elif defined(XP_MACOSX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSafariProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOmniWebProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMacIEProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCaminoProfileMigrator)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsICabProfileMigrator)
#endif
#ifdef MOZ_FEEDS
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFeedSniffer)
#endif
#ifdef MOZ_SAFE_BROWSING
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDocNavStartProgressListener)
#endif

/////////////////////////////////////////////////////////////////////////////

static const nsModuleComponentInfo components[] =
{
#if defined(XP_WIN)
  { "Browser Shell Service",
    NS_SHELLSERVICE_CID,
    NS_SHELLSERVICE_CONTRACTID,
    nsWindowsShellServiceConstructor,
    nsWindowsShellService::Register },

#elif defined(MOZ_WIDGET_GTK2)
  { "Browser Shell Service",
    NS_SHELLSERVICE_CID,
    NS_SHELLSERVICE_CONTRACTID,
    nsGNOMEShellServiceConstructor },

#endif

#if defined(MOZ_PLACES)
  { "Browser Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    NS_NAVHISTORYSERVICE_CONTRACTID,
    nsNavHistoryConstructor },

  { "Browser Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    "@mozilla.org/browser/global-history;2",
    nsNavHistoryConstructor },

  { "Browser Navigation History",
    NS_NAVHISTORYSERVICE_CID,
    "@mozilla.org/autocomplete/search;1?name=history",
    nsNavHistoryConstructor },

  { "History tree view",
    NS_NAVHISTORYRESULTTREEVIEWER_CID,
    NS_NAVHISTORYRESULTTREEVIEWER_CONTRACTID,
    nsNavHistoryResultTreeViewerConstructor },

  { "Page Annotation Service",
    NS_ANNOTATIONSERVICE_CID,
    NS_ANNOTATIONSERVICE_CONTRACTID,
    nsAnnotationServiceConstructor },

  { "Annotation Protocol Handler",
    NS_ANNOPROTOCOLHANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-anno",
    nsAnnoProtocolHandlerConstructor },

  { "Browser Bookmarks Service",
    NS_NAVBOOKMARKSSERVICE_CID,
    NS_NAVBOOKMARKSSERVICE_CONTRACTID,
    nsNavBookmarksConstructor },

  { "Favicon Service",
    NS_FAVICONSERVICE_CID,
    NS_FAVICONSERVICE_CONTRACTID,
    nsFaviconServiceConstructor },

  { "Livemark Service",
    NS_LIVEMARKSERVICE_CID,
    NS_LIVEMARKSERVICE_CONTRACTID,
    nsLivemarkServiceConstructor },

  { "Mork History Importer",
    NS_MORKHISTORYIMPORTER_CID,
    NS_MORKHISTORYIMPORTER_CONTRACTID,
    nsMorkHistoryImporterConstructor },

#else

  { "Bookmarks",
    NS_BOOKMARKS_SERVICE_CID,
    NS_BOOKMARKS_SERVICE_CONTRACTID,
    nsBookmarksServiceConstructor },

  { "Bookmarks",
    NS_BOOKMARKS_SERVICE_CID,
    NS_BOOKMARKS_DATASOURCE_CONTRACTID,
    nsBookmarksServiceConstructor },

  { "Bookmarks Forward Proxy Inference Data Source",
    NS_RDF_FORWARDPROXY_INFER_DATASOURCE_CID,
    NS_RDF_INFER_DATASOURCE_CONTRACTID_PREFIX "forward-proxy",
    nsForwardProxyDataSourceConstructor },

#endif

#ifdef MOZ_FEEDS
  { "Feed Sniffer",
    NS_FEEDSNIFFER_CID,
    NS_FEEDSNIFFER_CONTRACTID,
    nsFeedSnifferConstructor,
    nsFeedSniffer::Register },

  { "about:feeds Page",
    NS_ABOUTFEEDS_CID,
    NS_ABOUT_MODULE_CONTRACTID_PREFIX "feeds",
    nsAboutFeeds::Create
  },
#endif

#ifdef MOZ_SAFE_BROWSING
  { "Safe browsing document nav start progress listener",
    NS_DOCNAVSTARTPROGRESSLISTENER_CID,
    NS_DOCNAVSTARTPROGRESSLISTENER_CONTRACTID,
    nsDocNavStartProgressListenerConstructor },
#endif

  { "Profile Migrator",
    NS_FIREFOX_PROFILEMIGRATOR_CID,
    NS_PROFILEMIGRATOR_CONTRACTID,
    nsProfileMigratorConstructor },

#if defined(XP_WIN) && !defined(__MINGW32__)
  { "Internet Explorer (Windows) Profile Migrator",
    NS_WINIEPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "ie",
    nsIEProfileMigratorConstructor },

#elif defined(XP_MACOSX)
  { "Browser Shell Service",
    NS_SHELLSERVICE_CID,
    NS_SHELLSERVICE_CONTRACTID,
    nsMacShellServiceConstructor },

  { "Safari Profile Migrator",
    NS_SAFARIPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "safari",
    nsSafariProfileMigratorConstructor },

  { "Internet Explorer (Macintosh) Profile Migrator",
    NS_MACIEPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "macie",
    nsMacIEProfileMigratorConstructor },

  { "OmniWeb Profile Migrator",
    NS_OMNIWEBPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "omniweb",
    nsOmniWebProfileMigratorConstructor },

  { "Camino Profile Migrator",
    NS_CAMINOPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "camino",
    nsCaminoProfileMigratorConstructor },

  { "iCab Profile Migrator",
    NS_ICABPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "icab",
    nsICabProfileMigratorConstructor },

#endif

#if !defined(XP_OS2)
  { "Opera Profile Migrator",
    NS_OPERAPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "opera",
    nsOperaProfileMigratorConstructor },
#endif

#if !defined(XP_BEOS)
  { "Netscape 4.x Profile Migrator",
    NS_DOGBERTPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "dogbert",
    nsDogbertProfileMigratorConstructor },
#endif

  { "Phoenix Profile Migrator",
    NS_PHOENIXPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "phoenix",
    nsPhoenixProfileMigratorConstructor },

  { "Seamonkey Profile Migrator",
    NS_SEAMONKEYPROFILEMIGRATOR_CID,
    NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "seamonkey",
    nsSeamonkeyProfileMigratorConstructor }
};

NS_IMPL_NSGETMODULE(nsBrowserCompsModule, components)

