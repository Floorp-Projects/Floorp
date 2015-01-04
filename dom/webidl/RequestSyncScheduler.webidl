/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This is the dictionary for the creation of a new task.
dictionary RequestTaskParams {
  required USVString wakeUpPage;
  boolean oneShot = true;
  required long minInterval; // in seconds >= dom.requestSync.minInterval or 100 secs
  boolean wifiOnly = true;
  any data = null;
};


// This is the dictionary you can have back from registration{s}().
dictionary RequestTask : RequestTaskParams {
  USVString task = "";

  // Last synchonization date.. maybe it's useful to know.
  DOMTimeStamp lastSync;
};

[NavigatorProperty="sync",
 AvailableIn=CertifiedApps,
 Pref="dom.requestSync.enabled",
 JSImplementation="@mozilla.org/dom/request-sync-scheduler;1"]
interface RequestSyncScheduler {

  Promise<void> register(USVString task,
                         optional RequestTaskParams params);
  Promise<void> unregister(USVString task);

  // Useful methods to get registrations
  Promise<sequence<RequestTask>> registrations();
  Promise<RequestTask> registration(USVString task);
};
