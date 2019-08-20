/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import update, { initialSourcesState, getDisplayedSources } from "../sources";
import { initialThreadsState } from "../threads";
import updateSourceActors from "../source-actors";
import type { SourceActor } from "../../types";
import { prefs } from "../../utils/prefs";
import { makeMockSource, mockcx } from "../../utils/test-mockup";
import { getResourceIds } from "../../utils/resource";

const extensionSource = {
  ...makeMockSource(),
  id: "extensionId",
  url: "http://example.com/script.js",
};

const firefoxExtensionSource = {
  ...makeMockSource(),
  id: "firefoxExtension",
  url: "moz-extension://id/js/content.js",
  isExtension: true,
};

const chromeExtensionSource = {
  ...makeMockSource(),
  id: "chromeExtension",
  isExtension: true,
  url: "chrome-extension://id/js/content.js",
};

const mockedSources = [
  extensionSource,
  firefoxExtensionSource,
  chromeExtensionSource,
];

const mockSourceActors: Array<SourceActor> = ([
  {
    id: "extensionId-actor",
    actor: "extensionId-actor",
    source: "extensionId",
    thread: "foo",
  },
  {
    id: "firefoxExtension-actor",
    actor: "firefoxExtension-actor",
    source: "firefoxExtension",
    thread: "foo",
  },
  {
    id: "chromeExtension-actor",
    actor: "chromeExtension-actor",
    source: "chromeExtension",
    thread: "foo",
  },
]: any);

describe("sources reducer", () => {
  it("should work", () => {
    let state = initialSourcesState();
    state = update(state, {
      type: "ADD_SOURCE",
      cx: mockcx,
      source: makeMockSource(),
    });
    expect(getResourceIds(state.sources)).toHaveLength(1);
  });
});

describe("sources selectors", () => {
  it("should return all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtenstionsEnabled = true;
    let state = initialSourcesState();
    state = {
      sources: update(state, {
        type: "ADD_SOURCES",
        cx: mockcx,
        sources: mockedSources,
      }),
      sourceActors: undefined,
    };
    const insertAction = {
      type: "INSERT_SOURCE_ACTORS",
      items: mockSourceActors,
    };
    state = {
      sources: update(state.sources, insertAction),
      sourceActors: updateSourceActors(state.sourceActors, insertAction),
      threads: initialThreadsState(),
    };
    const threadSources = getDisplayedSources(state);
    expect(Object.values(threadSources.foo)).toHaveLength(3);
  });

  it("should omit all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtenstionsEnabled = false;
    let state = initialSourcesState();
    state = {
      sources: update(state, {
        type: "ADD_SOURCES",
        cx: mockcx,
        sources: mockedSources,
      }),
      sourceActors: undefined,
    };

    const insertAction = {
      type: "INSERT_SOURCE_ACTORS",
      items: mockSourceActors,
    };

    state = {
      sources: update(state.sources, insertAction),
      sourceActors: updateSourceActors(state.sourceActors, insertAction),
      threads: initialThreadsState(),
    };
    const threadSources = getDisplayedSources(state);
    expect(Object.values(threadSources.foo)).toHaveLength(1);
  });
});
