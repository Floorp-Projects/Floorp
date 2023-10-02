/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGNOMEShellDBusHelper_h__
#define __nsGNOMEShellDBusHelper_h__

#include <gio/gio.h>
#include "nsINavHistoryService.h"

#define MAX_SEARCH_RESULTS_NUM 9
#define KEYWORD_SEARCH_STRING "special:search"
#define KEYWORD_SEARCH_STRING_LEN 14

class nsGNOMEShellHistorySearchResult;

void DBusHandleResultSet(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
                         GVariant* aParameters, bool aInitialSearch,
                         GDBusMethodInvocation* aReply);
void DBusHandleResultMetas(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
    GVariant* aParameters, GDBusMethodInvocation* aReply);
void DBusActivateResult(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
                        GVariant* aParameters, GDBusMethodInvocation* aReply);
void DBusLaunchSearch(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
                      GVariant* aParameters, GDBusMethodInvocation* aReply);
bool IsHistoryResultNodeURI(nsINavHistoryResultNode* aHistoryNode);

const char* GetDBusBusName();
const char* GetDBusObjectPath();

#endif  // __nsGNOMEShellDBusHelper_h__
