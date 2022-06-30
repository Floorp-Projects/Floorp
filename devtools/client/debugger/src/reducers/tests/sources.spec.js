/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import update, { initialSourcesState } from "../sources";
import { getDisplayedSources } from "../../selectors/sources";
import { initialThreadsState } from "../threads";
import updateSourceActors from "../source-actors";
import { prefs } from "../../utils/prefs";
import { makeMockSource, mockcx, makeMockState } from "../../utils/test-mockup";
import { getDisplayURL } from "../../utils/sources-tree/getURL";

const extensionSource = {
  ...makeMockSource(),
  id: "extensionId",
  url: "http://example.com/script.js",
  displayURL: getDisplayURL("http://example.com/script.js"),
  thread: "foo",
};

const firefoxExtensionSource = {
  ...makeMockSource(),
  id: "firefoxExtension",
  url: "moz-extension://id/js/content.js",
  displayURL: getDisplayURL("moz-extension://id/js/content.js"),
  isExtension: true,
  thread: "foo",
};

const chromeExtensionSource = {
  ...makeMockSource(),
  id: "chromeExtension",
  isExtension: true,
  url: "chrome-extension://id/js/content.js",
  displayURL: getDisplayURL("chrome-extension://id/js/content.js"),
  thread: "foo",
};

const mockedSources = [
  extensionSource,
  firefoxExtensionSource,
  chromeExtensionSource,
];

const mockSourceActors = [
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
];

describe("sources reducer", () => {
  it("should work", () => {
    let state = initialSourcesState();
    state = update(state, {
      type: "ADD_SOURCES",
      cx: mockcx,
      sources: [makeMockSource()],
    });
    expect(state.sources.size).toBe(1);
  });
});

describe("sources selectors", () => {
  it("should return all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtensionsEnabled = true;
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
    state = makeMockState({
      sources: update(state.sources, insertAction),
      sourceActors: updateSourceActors(state.sourceActors, insertAction),
      threads: initialThreadsState(),
    });
    const threadSources = getDisplayedSources(state);
    expect(Object.values(threadSources.foo)).toHaveLength(3);
  });

  it("should omit all extensions when chrome preference enabled", () => {
    prefs.chromeAndExtensionsEnabled = false;
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

    state = makeMockState({
      sources: update(state.sources, insertAction),
      sourceActors: updateSourceActors(state.sourceActors, insertAction),
      threads: initialThreadsState(),
    });
    const threadSources = getDisplayedSources(state);
    expect(Object.values(threadSources.foo)).toHaveLength(1);
  });
});
