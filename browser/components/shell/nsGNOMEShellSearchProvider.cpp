/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGNOMEShellSearchProvider.h"
#include "nsGNOMEShellDBusHelper.h"

#include "nsIWidget.h"
#include "nsToolkitCompsCID.h"
#include "nsIFaviconService.h"
#include "RemoteUtils.h"
#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

DBusHandlerResult nsGNOMEShellSearchProvider::HandleSearchResultSet(
    DBusMessage* aMsg, bool aInitialSearch) {
  // Discard any existing search results.
  mSearchResult = nullptr;

  RefPtr<nsGNOMEShellHistorySearchResult> newSearch =
      new nsGNOMEShellHistorySearchResult(this, mConnection,
                                          mSearchResultTimeStamp);
  mSearchResultTimeStamp++;
  newSearch->SetTimeStamp(mSearchResultTimeStamp);

  // Send the search request over DBus. We'll get reply over DBus it will be
  // set to mSearchResult by nsGNOMEShellSearchProvider::SetSearchResult().
  return aInitialSearch
             ? DBusHandleInitialResultSet(newSearch.forget(), aMsg)
             : DBusHandleSubsearchResultSet(newSearch.forget(), aMsg);
}

DBusHandlerResult nsGNOMEShellSearchProvider::HandleResultMetas(
    DBusMessage* aMsg) {
  if (!mSearchResult) {
    NS_WARNING("Missing nsGNOMEShellHistorySearchResult.");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBusHandleResultMetas(mSearchResult, aMsg);
}

DBusHandlerResult nsGNOMEShellSearchProvider::ActivateResult(
    DBusMessage* aMsg) {
  if (!mSearchResult) {
    NS_WARNING("Missing nsGNOMEShellHistorySearchResult.");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBusActivateResult(mSearchResult, aMsg);
}

DBusHandlerResult nsGNOMEShellSearchProvider::LaunchSearch(DBusMessage* aMsg) {
  if (!mSearchResult) {
    NS_WARNING("Missing nsGNOMEShellHistorySearchResult.");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBusLaunchSearch(mSearchResult, aMsg);
}

DBusHandlerResult nsGNOMEShellSearchProvider::HandleDBusMessage(
    DBusConnection* aConnection, DBusMessage* aMsg) {
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");

  const char* method = dbus_message_get_member(aMsg);
  const char* iface = dbus_message_get_interface(aMsg);

  if ((strcmp("Introspect", method) == 0) &&
      (strcmp("org.freedesktop.DBus.Introspectable", iface) == 0)) {
    return DBusIntrospect(mConnection, aMsg);
  }

  if (strcmp("org.gnome.Shell.SearchProvider2", iface) == 0) {
    if (strcmp("GetInitialResultSet", method) == 0) {
      return HandleSearchResultSet(aMsg, /* aInitialSearch */ true);
    }
    if (strcmp("GetSubsearchResultSet", method) == 0) {
      return HandleSearchResultSet(aMsg, /* aInitialSearch */ false);
    }
    if (strcmp("GetResultMetas", method) == 0) {
      return HandleResultMetas(aMsg);
    }
    if (strcmp("ActivateResult", method) == 0) {
      return ActivateResult(aMsg);
    }
    if (strcmp("LaunchSearch", method) == 0) {
      return LaunchSearch(aMsg);
    }
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void nsGNOMEShellSearchProvider::UnregisterDBusInterface(
    DBusConnection* aConnection) {
  NS_ASSERTION(mConnection == aConnection, "Wrong D-Bus connection.");
  // Not implemented
}

static DBusHandlerResult message_handler(DBusConnection* conn,
                                         DBusMessage* aMsg, void* user_data) {
  auto interface = static_cast<nsGNOMEShellSearchProvider*>(user_data);
  return interface->HandleDBusMessage(conn, aMsg);
}

static void unregister(DBusConnection* conn, void* user_data) {
  auto interface = static_cast<nsGNOMEShellSearchProvider*>(user_data);
  interface->UnregisterDBusInterface(conn);
}

static DBusObjectPathVTable remoteHandlersTable = {
    .unregister_function = unregister,
    .message_function = message_handler,
};

nsresult nsGNOMEShellSearchProvider::Startup() {
  if (mConnection && dbus_connection_get_is_connected(mConnection)) {
    // We're already connected so we don't need to reconnect
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mConnection =
      already_AddRefed<DBusConnection>(dbus_bus_get(DBUS_BUS_SESSION, nullptr));
  if (!mConnection) {
    return NS_ERROR_FAILURE;
  }
  dbus_connection_set_exit_on_disconnect(mConnection, false);
  dbus_connection_setup_with_g_main(mConnection, nullptr);

  DBusError err;
  dbus_error_init(&err);
  dbus_bus_request_name(mConnection, DBUS_BUS_NAME, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                        &err);
  // The interface is already owned - there is another application/profile
  // instance already running.
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }

  if (!dbus_connection_register_object_path(mConnection, DBUS_OBJECT_PATH,
                                            &remoteHandlersTable, this)) {
    mConnection = nullptr;
    return NS_ERROR_FAILURE;
  }

  mSearchResultTimeStamp = 0;
  return NS_OK;
}

void nsGNOMEShellSearchProvider::Shutdown() {
  if (!mConnection) {
    return;
  }

  dbus_connection_unregister_object_path(mConnection, DBUS_OBJECT_PATH);

  // dbus_connection_unref() will be called by RefPtr here.
  mConnection = nullptr;
}

bool nsGNOMEShellSearchProvider::SetSearchResult(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult) {
  MOZ_ASSERT(!mSearchResult);

  if (mSearchResultTimeStamp != aSearchResult->GetTimeStamp()) {
    NS_WARNING("Time stamp mismatch.");
    return false;
  }
  mSearchResult = aSearchResult;
  return true;
}

static void DispatchSearchResults(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
    nsCOMPtr<nsINavHistoryContainerResultNode> aHistResultContainer) {
  aSearchResult->SetSearchResultContainer(aHistResultContainer);
}

nsresult nsGNOMEShellHistoryService::QueryHistory(
    RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult) {
  if (!mHistoryService) {
    mHistoryService = do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
    if (!mHistoryService) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv;
  nsCOMPtr<nsINavHistoryQuery> histQuery;
  rv = mHistoryService->GetNewQuery(getter_AddRefs(histQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = histQuery->SetSearchTerms(
      NS_ConvertUTF8toUTF16(aSearchResult->GetSearchTerm()));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryQueryOptions> histQueryOpts;
  rv = mHistoryService->GetNewQueryOptions(getter_AddRefs(histQueryOpts));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = histQueryOpts->SetSortingMode(
      nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = histQueryOpts->SetMaxResults(MAX_SEARCH_RESULTS_NUM);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryResult> histResult;
  rv = mHistoryService->ExecuteQuery(histQuery, histQueryOpts,
                                     getter_AddRefs(histResult));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryContainerResultNode> resultContainer;

  rv = histResult->GetRoot(getter_AddRefs(resultContainer));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = resultContainer->SetContainerOpen(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Simulate async searching by delayed reply. This search API will
  // likely become async in the future and we want to be sure to not rely on
  // its current synchronous behavior.
  MOZ_ASSERT(MessageLoop::current());
  MessageLoop::current()->PostTask(
      NewRunnableFunction("Gnome shell search results", &DispatchSearchResults,
                          aSearchResult, resultContainer));

  return NS_OK;
}

static void DBusGetIDKeyForURI(int aIndex, nsAutoCString& aUri,
                               nsAutoCString& aIDKey) {
  // Compose ID as NN:URL where NN is index to our current history
  // result container.
  aIDKey = nsPrintfCString("%.2d:%s", aIndex, aUri.get());
}

void nsGNOMEShellHistorySearchResult::SendDBusSearchResultReply() {
  MOZ_ASSERT(mReply);

  uint32_t childCount = 0;
  nsresult rv = mHistResultContainer->GetChildCount(&childCount);

  DBusMessageIter iter;
  dbus_message_iter_init_append(mReply, &iter);
  DBusMessageIter iterArray;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &iterArray);

  if (NS_SUCCEEDED(rv) && childCount > 0) {
    if (childCount > MAX_SEARCH_RESULTS_NUM) {
      childCount = MAX_SEARCH_RESULTS_NUM;
    }

    for (uint32_t i = 0; i < childCount; i++) {
      nsCOMPtr<nsINavHistoryResultNode> child;
      mHistResultContainer->GetChild(i, getter_AddRefs(child));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }
      if (!IsHistoryResultNodeURI(child)) {
        continue;
      }

      nsAutoCString uri;
      child->GetUri(uri);

      nsAutoCString idKey;
      DBusGetIDKeyForURI(i, uri, idKey);

      const char* id = idKey.get();
      dbus_message_iter_append_basic(&iterArray, DBUS_TYPE_STRING, &id);
    }
  }

  nsPrintfCString searchString("%s:%s", KEYWORD_SEARCH_STRING,
                               mSearchTerm.get());
  const char* search = searchString.get();
  dbus_message_iter_append_basic(&iterArray, DBUS_TYPE_STRING, &search);

  dbus_message_iter_close_container(&iter, &iterArray);

  dbus_connection_send(mConnection, mReply, nullptr);
  dbus_message_unref(mReply);

  mReply = nullptr;
}

void nsGNOMEShellHistorySearchResult::SetSearchResultContainer(
    nsCOMPtr<nsINavHistoryContainerResultNode> aHistResultContainer) {
  if (mSearchProvider->SetSearchResult(this)) {
    mHistResultContainer = aHistResultContainer;
    SendDBusSearchResultReply();
  }
}

nsGNOMEShellHistoryService* GetGNOMEShellHistoryService() {
  static nsGNOMEShellHistoryService gGNOMEShellHistoryService;
  return &gGNOMEShellHistoryService;
}
