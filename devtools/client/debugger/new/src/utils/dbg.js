/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import * as timings from "./timings";
import { prefs, asyncStore, features } from "./prefs";
import { isDevelopment, isTesting } from "devtools-environment";

function findSource(dbg: any, url: string) {
  const sources = dbg.selectors.getSourceList();
  return sources.find(s => (s.url || "").includes(url));
}

function findSources(dbg: any, url: string) {
  const sources = dbg.selectors.getSourceList();
  return sources.filter(s => (s.url || "").includes(url));
}

function sendPacket(dbg: any, packet: any) {
  return dbg.client.sendPacket(packet);
}

function sendPacketToThread(dbg: Object, packet: any) {
  return sendPacket(dbg, {
    to: dbg.connection.tabConnection.threadClient.actor,
    ...packet
  });
}

function evaluate(dbg: Object, expression: any) {
  return dbg.client.evaluate(expression);
}

function bindSelectors(obj: Object): Object {
  return Object.keys(obj.selectors).reduce((bound, selector) => {
    bound[selector] = (a, b, c) =>
      obj.selectors[selector](obj.store.getState(), a, b, c);
    return bound;
  }, {});
}

function getCM() {
  const cm: any = document.querySelector(".CodeMirror");
  return cm && cm.CodeMirror;
}

function _formatColumnBreapoints(dbg: Object) {
  console.log(
    dbg.selectors.formatColumnBreakpoints(
      dbg.selectors.visibleColumnBreakpoints()
    )
  );
}

export function setupHelper(obj: Object) {
  const selectors = bindSelectors(obj);
  const dbg: Object = {
    ...obj,
    selectors,
    prefs,
    asyncStore,
    features,
    timings,
    getCM,
    helpers: {
      findSource: url => findSource(dbg, url),
      findSources: url => findSources(dbg, url),
      evaluate: expression => evaluate(dbg, expression),
      sendPacketToThread: packet => sendPacketToThread(dbg, packet),
      sendPacket: packet => sendPacket(dbg, packet),
      dumpThread: () => sendPacketToThread(dbg, { type: "dumpThread" })
    },
    formatters: {
      visibleColumnBreakpoints: () => _formatColumnBreapoints(dbg)
    },
    _telemetry: {
      events: {}
    }
  };

  window.dbg = dbg;

  if (isDevelopment() && !isTesting()) {
    console.group("Development Notes");
    const baseUrl = "https://firefox-devtools.github.io/debugger";
    const localDevelopmentUrl = `${baseUrl}/docs/dbg.html`;
    console.log("Debugging Tips", localDevelopmentUrl);
    console.log("dbg", window.dbg);
    console.groupEnd();
  }
}
