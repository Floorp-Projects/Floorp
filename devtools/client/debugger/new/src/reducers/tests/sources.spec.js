/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import update, { initialSourcesState, getRelativeSources } from "../sources";
import { foobar } from "../../test/fixtures";
import type { Source } from "../../types";
import { prefs } from "../../utils/prefs";

const fakeSources = foobar.sources.sources;

const extenstionSource = {
  id: "extenstionId",
  url: "http://example.com/script.js"
};

const firefoxExtensionSource = {
  id: "firefoxExtension",
  url: "moz-extension://id/js/content.js"
};

const chromeExtensionSource = {
  id: "chromeExtension",
  url: "chrome-extension://id/js/content.js"
};

const mockedSources = [
  extenstionSource,
  firefoxExtensionSource,
  chromeExtensionSource
];

const mockedSourceActors = [
  { actor: "extensionId-actor", source: "extenstionId", thread: "foo" },
  {
    actor: "firefoxExtension-actor",
    source: "firefoxExtension",
    thread: "foo"
  },
  { actor: "chromeExtension-actor", source: "chromeExtension", thread: "foo" }
];

describe("sources reducer", () => {
  it("should work", () => {
    let state = initialSourcesState();
    state = update(state, {
      type: "ADD_SOURCE",
      // coercing to a Source for the purpose of this test
      source: ((fakeSources.fooSourceActor: any): Source)
    });
    expect(Object.keys(state.sources)).toHaveLength(1);
  });
});

describe("sources selectors", () => {
  it("should return all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtenstionsEnabled = true;
    let state = initialSourcesState();
    state = {
      sources: update(state, {
        type: "ADD_SOURCES",
        // coercing to a Source for the purpose of this test
        sources: ((mockedSources: any): Source[]),
        sourceActors: mockedSourceActors
      })
    };
    const selectedRelativeSources = getRelativeSources(state);
    const threadSources = selectedRelativeSources.foo;
    expect(Object.values(threadSources)).toHaveLength(3);
  });

  it("should omit all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtenstionsEnabled = false;
    let state = initialSourcesState();
    state = {
      sources: update(state, {
        type: "ADD_SOURCES",
        // coercing to a Source for the purpose of this test
        sources: ((mockedSources: any): Source[]),
        sourceActors: mockedSourceActors
      })
    };
    const selectedRelativeSources = getRelativeSources(state);
    const threadSources = selectedRelativeSources.foo;
    expect(Object.values(threadSources)).toHaveLength(1);
  });
});
