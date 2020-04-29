/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsGNOMEShellSearchProvider_h__
#define __nsGNOMEShellSearchProvider_h__

#include "mozilla/DBusHelpers.h"
#include "nsINavHistoryService.h"
#include "nsUnixRemoteServer.h"
#include "nsCOMPtr.h"

class nsGNOMEShellSearchProvider;

// nsGNOMEShellHistorySearchResult is a container with contains search results
// which are files asynchronously by nsGNOMEShellHistoryService.
// The search results can be opened by Firefox then.
class nsGNOMEShellHistorySearchResult : public nsUnixRemoteServer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsGNOMEShellHistorySearchResult)

  nsGNOMEShellHistorySearchResult(nsGNOMEShellSearchProvider* aSearchProvider,
                                  DBusConnection* aConnection, int aTimeStamp)
      : mSearchProvider(aSearchProvider),
        mReply(nullptr),
        mConnection(aConnection),
        mTimeStamp(aTimeStamp){};

  void SetReply(DBusMessage* aReply) { mReply = aReply; }
  void SetSearchTerm(const char* aSearchTerm) {
    mSearchTerm = nsAutoCString(aSearchTerm);
  }
  DBusConnection* GetDBusConnection() { return mConnection; }
  int GetTimeStamp() { return mTimeStamp; }
  void SetTimeStamp(int aTimeStamp) { mTimeStamp = aTimeStamp; }
  nsAutoCString& GetSearchTerm() { return mSearchTerm; }
  void SetSearchResultContainer(
      nsCOMPtr<nsINavHistoryContainerResultNode> aHistResultContainer);
  nsCOMPtr<nsINavHistoryContainerResultNode> GetSearchResultContainer() {
    return mHistResultContainer;
  }
  void HandleCommandLine(const char* aBuffer, uint32_t aTimestamp) {
    nsUnixRemoteServer::HandleCommandLine(aBuffer, aTimestamp);
  }

 private:
  void SendDBusSearchResultReply();

  ~nsGNOMEShellHistorySearchResult() = default;

 private:
  nsGNOMEShellSearchProvider* mSearchProvider;
  nsCOMPtr<nsINavHistoryContainerResultNode> mHistResultContainer;
  nsAutoCString mSearchTerm;
  DBusMessage* mReply;
  DBusConnection* mConnection;
  int mTimeStamp;
};

class nsGNOMEShellHistoryService {
 public:
  nsresult QueryHistory(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult);

 private:
  nsCOMPtr<nsINavHistoryService> mHistoryService;
};

class nsGNOMEShellSearchProvider {
 public:
  nsGNOMEShellSearchProvider()
      : mConnection(nullptr), mSearchResultTimeStamp(0) {}
  ~nsGNOMEShellSearchProvider() { Shutdown(); }

  nsresult Startup();
  void Shutdown();

  DBusHandlerResult HandleDBusMessage(DBusConnection* aConnection,
                                      DBusMessage* msg);
  void UnregisterDBusInterface(DBusConnection* aConnection);

  bool SetSearchResult(RefPtr<nsGNOMEShellHistorySearchResult> aSearchResult);

 private:
  DBusHandlerResult HandleSearchResultSet(DBusMessage* msg,
                                          bool aInitialSearch);
  DBusHandlerResult HandleResultMetas(DBusMessage* msg);
  DBusHandlerResult ActivateResult(DBusMessage* msg);
  DBusHandlerResult LaunchSearch(DBusMessage* msg);

  // The connection is owned by DBus library
  RefPtr<DBusConnection> mConnection;
  RefPtr<nsGNOMEShellHistorySearchResult> mSearchResult;
  int mSearchResultTimeStamp;
};

nsGNOMEShellHistoryService* GetGNOMEShellHistoryService();

#endif  // __nsGNOMEShellSearchProvider_h__
