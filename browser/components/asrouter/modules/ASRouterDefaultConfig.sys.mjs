/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ASRouter } from "resource:///modules/asrouter/ASRouter.sys.mjs";
import { TelemetryFeed } from "resource://activity-stream/lib/TelemetryFeed.sys.mjs";
import { ASRouterParentProcessMessageHandler } from "resource:///modules/asrouter/ASRouterParentProcessMessageHandler.sys.mjs";

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment does not actually rely
// on SpecialMessageActions, and overrides importESModule to be
// a no-op (which can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

import { ASRouterPreferences } from "resource:///modules/asrouter/ASRouterPreferences.sys.mjs";
import { QueryCache } from "resource:///modules/asrouter/ASRouterTargeting.sys.mjs";
import { ActivityStreamStorage } from "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs";

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
