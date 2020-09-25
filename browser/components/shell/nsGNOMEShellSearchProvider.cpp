/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGNOMEShellSearchProvider.h"

#include "nsIWidget.h"
#include "nsToolkitCompsCID.h"
#include "nsIFaviconService.h"
#include "RemoteUtils.h"
#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "nsIServiceManager.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsIIOService.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "imgIContainer.h"
#include "imgITools.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"

using namespace mozilla;
using namespace mozilla::gfx;

class AsyncFaviconDataReady final : public nsIFaviconDataCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONDATACALLBACK

  AsyncFaviconDataReady(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult,
                        int aIconIndex, int aTimeStamp)
      : mSearchResult(aSearchResult),
        mIconIndex(aIconIndex),
        mTimeStamp(aTimeStamp){};

 private:
  ~AsyncFaviconDataReady() {}

  RefPtr<nsGNOMEShellHistorySearchResult> mSearchResult;
  int mIconIndex;
  int mTimeStamp;
};

NS_IMPL_ISUPPORTS(AsyncFaviconDataReady, nsIFaviconDataCallback)

// Inspired by SurfaceToPackedBGRA
static UniquePtr<uint8_t[]> SurfaceToPackedRGBA(DataSourceSurface* aSurface) {
  IntSize size = aSurface->GetSize();
  CheckedInt<size_t> bufferSize =
      CheckedInt<size_t>(size.width * 4) * CheckedInt<size_t>(size.height);
  if (!bufferSize.isValid()) {
    return nullptr;
  }
  UniquePtr<uint8_t[]> imageBuffer(new (std::nothrow)
                                       uint8_t[bufferSize.value()]);
  if (!imageBuffer) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }

  // Convert BGRA to RGBA
  uint32_t* aSrc = (uint32_t*)map.mData;
  uint32_t* aDst = (uint32_t*)imageBuffer.get();
  for (int i = 0; i < size.width * size.height; i++, aDst++, aSrc++) {
    *aDst = *aSrc & 0xff00ff00;
    *aDst |= (*aSrc & 0xff) << 16;
    *aDst |= (*aSrc & 0xff0000) >> 16;
  }

  aSurface->Unmap();

  return imageBuffer;
}

NS_IMETHODIMP
AsyncFaviconDataReady::OnComplete(nsIURI* aFaviconURI, uint32_t aDataLen,
                                  const uint8_t* aData,
                                  const nsACString& aMimeType,
                                  uint16_t aWidth) {
  // This is a callback from some previous search so we don't want it
  if (mTimeStamp != mSearchResult->GetTimeStamp() || !aData || !aDataLen) {
    return NS_ERROR_FAILURE;
  }

  // Decode the image from the format it was returned to us in (probably PNG)
  nsCOMPtr<imgIContainer> container;
  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");
  nsresult rv = imgtool->DecodeImageFromBuffer(
      reinterpret_cast<const char*>(aData), aDataLen, aMimeType,
      getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<SourceSurface> surface = container->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);

  if (!surface || surface->GetFormat() != SurfaceFormat::B8G8R8A8) {
    return NS_ERROR_FAILURE;
  }

  // Allocate a new buffer that we own.
  RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
  UniquePtr<uint8_t[]> data = SurfaceToPackedRGBA(dataSurface);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mSearchResult->SetHistoryIcon(mTimeStamp, std::move(data),
                                surface->GetSize().width,
                                surface->GetSize().height, mIconIndex);
  return NS_OK;
}

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
  aSearchResult->ReceiveSearchResultContainer(aHistResultContainer);
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

void nsGNOMEShellHistorySearchResult::HandleSearchResultReply() {
  MOZ_ASSERT(mReply);

  uint32_t childCount = 0;
  nsresult rv = mHistResultContainer->GetChildCount(&childCount);

  DBusMessageIter iter;
  dbus_message_iter_init_append(mReply, &iter);
  DBusMessageIter iterArray;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &iterArray);

  if (NS_SUCCEEDED(rv) && childCount > 0) {
    // Obtain the favicon service and get the favicon for the specified page
    nsCOMPtr<nsIFaviconService> favIconSvc(
        do_GetService("@mozilla.org/browser/favicon-service;1"));
    nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));

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

      nsCOMPtr<nsIURI> iconIri;
      ios->NewURI(uri, nullptr, nullptr, getter_AddRefs(iconIri));
      nsCOMPtr<nsIFaviconDataCallback> callback =
          new AsyncFaviconDataReady(this, i, mTimeStamp);
      favIconSvc->GetFaviconDataForPage(iconIri, callback, 0);

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

void nsGNOMEShellHistorySearchResult::ReceiveSearchResultContainer(
    nsCOMPtr<nsINavHistoryContainerResultNode> aHistResultContainer) {
  // Propagate search results to nsGNOMEShellSearchProvider.
  // SetSearchResult() checks this is up-to-date search (our time stamp matches
  // latest requested search timestamp).
  if (mSearchProvider->SetSearchResult(this)) {
    mHistResultContainer = aHistResultContainer;
    HandleSearchResultReply();
  }
}

void nsGNOMEShellHistorySearchResult::SetHistoryIcon(int aTimeStamp,
                                                     UniquePtr<uint8_t[]> aData,
                                                     int aWidth, int aHeight,
                                                     int aIconIndex) {
  MOZ_ASSERT(mTimeStamp == aTimeStamp);
  MOZ_RELEASE_ASSERT(aIconIndex < MAX_SEARCH_RESULTS_NUM);
  mHistoryIcons[aIconIndex].Set(mTimeStamp, std::move(aData), aWidth, aHeight);
}

GnomeHistoryIcon* nsGNOMEShellHistorySearchResult::GetHistoryIcon(
    int aIconIndex) {
  MOZ_RELEASE_ASSERT(aIconIndex < MAX_SEARCH_RESULTS_NUM);
  if (mHistoryIcons[aIconIndex].GetTimeStamp() == mTimeStamp &&
      mHistoryIcons[aIconIndex].IsLoaded()) {
    return mHistoryIcons + aIconIndex;
  }
  return nullptr;
}

nsGNOMEShellHistoryService* GetGNOMEShellHistoryService() {
  static nsGNOMEShellHistoryService gGNOMEShellHistoryService;
  return &gGNOMEShellHistoryService;
}
