/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[AvailableIn=CertifiedApps,
 Pref="dom.requestSync.enabled",
 CheckPermissions="requestsync-manager",
 JSImplementation="@mozilla.org/dom/request-sync-task-app;1"]
interface RequestSyncApp {
  readonly attribute USVString origin;
  readonly attribute USVString manifestURL;
  readonly attribute boolean isInBrowserElement;
};

enum RequestSyncTaskPolicyState { "enabled", "disabled", "wifiOnly" };

// Like a normal task, but with info about the app.
[AvailableIn=CertifiedApps,
 Pref="dom.requestSync.enabled",
 CheckPermissions="requestsync-manager",
 JSImplementation="@mozilla.org/dom/request-sync-task-manager;1"]
interface RequestSyncTask {
  // This object describes the app that is owning the task.
  readonly attribute RequestSyncApp app;

  // Using setPolicy it's possible to owerwrite the state and the minInterval.
  readonly attribute RequestSyncTaskPolicyState state;
  readonly attribute long overwrittenMinInterval;

  // These attributes are taken from the configuration of the task:

  readonly attribute USVString task;
  readonly attribute DOMTimeStamp lastSync;
  readonly attribute USVString wakeUpPage;
  readonly attribute boolean oneShot;
  readonly attribute long minInterval;
  readonly attribute boolean wifiOnly;
  readonly attribute any data;

  Promise<void> setPolicy(RequestSyncTaskPolicyState aState,
                          optional long ovewrittenMinInterval);
};

[NavigatorProperty="syncManager",
 AvailableIn=CertifiedApps,
 Pref="dom.requestSync.enabled",
 CheckPermissions="requestsync-manager",
 JSImplementation="@mozilla.org/dom/request-sync-manager;1"]
// This interface will be used only by the B2G SystemApp
interface RequestSyncManager {
    Promise<sequence<RequestSyncTask>> registrations();
};
