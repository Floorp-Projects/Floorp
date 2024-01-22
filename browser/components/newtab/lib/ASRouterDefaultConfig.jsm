/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ASRouterDefaultConfig"];

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { TelemetryFeed } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/TelemetryFeed.sys.mjs"
);
const { ASRouterParentProcessMessageHandler } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterParentProcessMessageHandler.jsm"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);
const { ASRouterPreferences } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterPreferences.jsm"
);
const { QueryCache } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
const { ActivityStreamStorage } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamStorage.jsm"
);

const createStorage = async telemetryFeed => {
  // "snippets" is the name of one storage space, but these days it is used
  // not for snippet-related data (snippets were removed in bug 1715158),
  // but storage for impression or session data for all ASRouter messages.
  //
  // We keep the name "snippets" to avoid having to do an IndexedDB database
  // migration.
  const dbStore = new ActivityStreamStorage({
    storeNames: ["sectionPrefs", "snippets"],
    telemetry: {
      handleUndesiredEvent: e => telemetryFeed.SendASRouterUndesiredEvent(e),
    },
  });
  // Accessing the db causes the object stores to be created / migrated.
  // This needs to happen before other instances try to access the db, which
  // would update only a subset of the stores to the latest version.
  try {
    await dbStore.db; // eslint-disable-line no-unused-expressions
  } catch (e) {
    return Promise.reject(e);
  }
  return dbStore.getDbTable("snippets");
};

const ASRouterDefaultConfig = () => {
  const router = ASRouter;
  const telemetry = new TelemetryFeed();
  const messageHandler = new ASRouterParentProcessMessageHandler({
    router,
    preferences: ASRouterPreferences,
    specialMessageActions: SpecialMessageActions,
    queryCache: QueryCache,
    sendTelemetry: telemetry.onAction.bind(telemetry),
  });
  return {
    router,
    messageHandler,
    createStorage: createStorage.bind(null, telemetry),
  };
};
