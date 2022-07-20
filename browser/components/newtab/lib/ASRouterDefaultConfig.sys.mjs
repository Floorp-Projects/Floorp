/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { ASRouter } from "resource://activity-stream/lib/ASRouter.sys.mjs";
import { TelemetryFeed } from "resource://activity-stream/lib/TelemetryFeed.sys.mjs";
import { ASRouterParentProcessMessageHandler } from "resource://activity-stream/lib/ASRouterParentProcessMessageHandler.sys.mjs";

const { SpecialMessageActions } = ChromeUtils.import(
  "resource://messaging-system/lib/SpecialMessageActions.jsm"
);
import { ASRouterPreferences } from "resource://activity-stream/lib/ASRouterPreferences.sys.mjs";
import { QueryCache } from "resource://activity-stream/lib/ASRouterTargeting.sys.mjs";
import { ActivityStreamStorage } from "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs";

const createStorage = async telemetryFeed => {
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

export const ASRouterDefaultConfig = () => {
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
