/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import update, { initialSourcesState, getDisplayedSources } from "../sources";
import type { Source } from "../../types";
import { prefs } from "../../utils/prefs";
import { makeMockSource } from "../../utils/test-mockup";

const extensionSource = {
  ...makeMockSource(),
  id: "extensionId",
  url: "http://example.com/script.js",
  actors: [{ actor: "extensionId-actor", source: "extensionId", thread: "foo" }]
};

const firefoxExtensionSource = {
  ...makeMockSource(),
  id: "firefoxExtension",
  url: "moz-extension://id/js/content.js",
  isExtension: true,
  actors: [
    {
      actor: "firefoxExtension-actor",
      source: "firefoxExtension",
      thread: "foo"
    }
  ]
};

const chromeExtensionSource = {
  ...makeMockSource(),
  id: "chromeExtension",
  isExtension: true,
  url: "chrome-extension://id/js/content.js",
  actors: [
    { actor: "chromeExtension-actor", source: "chromeExtension", thread: "foo" }
  ]
};

const mockedSources = [
  extensionSource,
  firefoxExtensionSource,
  chromeExtensionSource
];

describe("sources reducer", () => {
  it("should work", () => {
    let state = initialSourcesState();
    state = update(state, {
      type: "ADD_SOURCE",
      source: makeMockSource()
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
        sources: ((mockedSources: any): Source[])
      })
    };
    const selectedDisplayedSources = getDisplayedSources(state);
    const threadSources = selectedDisplayedSources.foo;
    expect(Object.values(threadSources)).toHaveLength(3);
  });

  it("should omit all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtenstionsEnabled = false;
    let state = initialSourcesState();
    state = {
      sources: update(state, {
        type: "ADD_SOURCES",
        // coercing to a Source for the purpose of this test
        sources: ((mockedSources: any): Source[])
      })
    };
    const selectedDisplayedSources = getDisplayedSources(state);
    const threadSources = selectedDisplayedSources.foo;
    expect(Object.values(threadSources)).toHaveLength(1);
  });
});
