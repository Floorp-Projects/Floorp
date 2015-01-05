/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Rappresentation of the app in the RequestTaskFull.
dictionary RequestTaskApp {
  USVString origin;
  USVString manifestURL;
  boolean isInBrowserElement;
};

// Like a normal task, but with info about the app.
dictionary RequestTaskFull : RequestTask {
  RequestTaskApp app;
};

[NavigatorProperty="syncManager",
 AvailableIn=CertifiedApps,
 Pref="dom.requestSync.enabled",
 CheckPermissions="requestsync-manager",
 JSImplementation="@mozilla.org/dom/request-sync-manager;1"]
// This interface will be used only by the B2G SystemApp
interface RequestSyncManager {
    Promise<sequence<RequestTaskFull>> registrations();
};
