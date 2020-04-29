/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGNOMEShellDBusHelper_h__
#define __nsGNOMEShellDBusHelper_h__

#include "mozilla/DBusHelpers.h"
#include "nsIStringBundle.h"
#include "nsINavHistoryService.h"

#define MAX_SEARCH_RESULTS_NUM 9
#define KEYWORD_SEARCH_STRING "special:search"
#define KEYWORD_SEARCH_STRING_LEN 14

#define DBUS_BUS_NAME "org.mozilla.Firefox.SearchProvider"
#define DBUS_OBJECT_PATH "/org/mozilla/Firefox/SearchProvider"

DBusHandlerResult DBusIntrospect(DBusConnection* aConnection,
                                 DBusMessage* aMsg);
DBusHandlerResult DBusHandleInitialResultSet(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg);
DBusHandlerResult DBusHandleSubsearchResultSet(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg);
DBusHandlerResult DBusHandleResultMetas(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg);
DBusHandlerResult DBusActivateResult(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg);
DBusHandlerResult DBusLaunchSearch(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult, DBusMessage* aMsg);
bool IsHistoryResultNodeURI(nsINavHistoryResultNode* aHistoryNode);

#endif  // __nsGNOMEShellDBusHelper_h__
