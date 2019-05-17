/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import * as timings from "./timings";
import { prefs, asyncStore, features } from "./prefs";
import { isDevelopment, isTesting } from "devtools-environment";
import { getDocument } from "./editor/source-documents";

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
    ...packet,
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

function formatMappedLocation(mappedLocation) {
  const { location, generatedLocation } = mappedLocation;
  return {
    original: `(${location.line}, ${location.column})`,
    generated: `(${generatedLocation.line}, ${generatedLocation.column})`,
  };
}

function formatMappedLocations(locations) {
  return console.table(locations.map(loc => formatMappedLocation(loc)));
}

function formatSelectedColumnBreakpoints(dbg) {
  const positions = dbg.selectors.getBreakpointPositionsForSource(
    dbg.selectors.getSelectedSource().id
  );

  return formatMappedLocations(positions);
}

function getDocumentForUrl(dbg, url) {
  const source = findSource(dbg, url);
  return getDocument(source.id);
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
      dumpThread: () => sendPacketToThread(dbg, { type: "dumpThread" }),
      getDocument: url => getDocumentForUrl(dbg, url),
    },
    formatters: {
      mappedLocations: locations => formatMappedLocations(locations),
      mappedLocation: location => formatMappedLocation(location),
      selectedColumnBreakpoints: () => formatSelectedColumnBreakpoints(dbg),
    },
    _telemetry: {
      events: {},
    },
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
