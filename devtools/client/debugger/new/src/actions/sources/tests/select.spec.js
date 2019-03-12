/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSymbols } from "../../../reducers/ast";
import {
  actions,
  selectors,
  createStore,
  makeFrame,
  makeSource,
  waitForState,
  makeOriginalSource
} from "../../../utils/test-head";
const {
  getSource,
  getSourceCount,
  getSelectedSource,
  getSourceTabs,
  getSourceMetaData,
  getOutOfScopeLocations,
  getSelectedLocation
} = selectors;

import { sourceThreadClient } from "../../tests/helpers/threadClient.js";

process.on("unhandledRejection", (reason, p) => {});

function initialLocation(sourceId) {
  return { sourceId, line: 1 };
}

describe("sources", () => {
  it("should select a source", async () => {
    // Note that we pass an empty client in because the action checks
    // if it exists.
    const store = createStore(sourceThreadClient);
    const { dispatch, getState } = store;

    const frame = makeFrame({ id: "1", sourceId: "foo1" });

    await dispatch(actions.newSource(makeSource("foo1")));
    await dispatch(
      actions.paused({
        thread: "FakeThread",
        why: { type: "debuggerStatement" },
        frame,
        frames: [frame]
      })
    );

    await dispatch(
      actions.selectLocation({ sourceId: "foo1", line: 1, column: 5 })
    );

    const selectedSource = getSelectedSource(getState());
    if (!selectedSource) {
      throw new Error("bad selectedSource");
    }
    expect(selectedSource.id).toEqual("foo1");

    const source = getSource(getState(), selectedSource.id);
    if (!source) {
      throw new Error("bad source");
    }
    expect(source.id).toEqual("foo1");

    await waitForState(
      store,
      state =>
        getOutOfScopeLocations(state) && getSourceMetaData(state, source.id)
    );
    const locations = getOutOfScopeLocations(getState());
    expect(locations).toHaveLength(1);
  });

  it("should select next tab on tab closed if no previous tab", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    const fooSource = makeSource("foo.js");

    await dispatch(actions.newSource(fooSource));
    await dispatch(actions.newSource(makeSource("bar.js")));
    await dispatch(actions.newSource(makeSource("baz.js")));

    // 3rd tab
    await dispatch(actions.selectLocation(initialLocation("foo.js")));

    // 2nd tab
    await dispatch(actions.selectLocation(initialLocation("bar.js")));

    // 1st tab
    await dispatch(actions.selectLocation(initialLocation("baz.js")));

    // 3rd tab is reselected
    await dispatch(actions.selectLocation(initialLocation("foo.js")));

    // closes the 1st tab, which should have no previous tab
    await dispatch(actions.closeTab(fooSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bar.js");
    expect(getSourceTabs(getState())).toHaveLength(2);
  });

  it("should open a tab for the source", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);
    await dispatch(actions.newSource(makeSource("foo.js")));
    dispatch(actions.selectLocation(initialLocation("foo.js")));

    const tabs = getSourceTabs(getState());
    expect(tabs).toHaveLength(1);
    expect(tabs[0].url).toEqual("http://localhost:8000/examples/foo.js");
  });

  it("should select previous tab on tab closed", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);
    await dispatch(actions.newSource(makeSource("foo.js")));
    await dispatch(actions.newSource(makeSource("bar.js")));

    const bazSource = makeSource("baz.js");
    await dispatch(actions.newSource(bazSource));

    await dispatch(actions.selectLocation(initialLocation("foo.js")));
    await dispatch(actions.selectLocation(initialLocation("bar.js")));
    await dispatch(actions.selectLocation(initialLocation("baz.js")));
    await dispatch(actions.closeTab(bazSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bar.js");
    expect(getSourceTabs(getState())).toHaveLength(2);
  });

  it("should keep the selected source when other tab closed", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    const bazSource = makeSource("baz.js");

    await dispatch(actions.newSource(makeSource("foo.js")));
    await dispatch(actions.newSource(makeSource("bar.js")));
    await dispatch(actions.newSource(bazSource));

    // 3rd tab
    await dispatch(actions.selectLocation(initialLocation("foo.js")));

    // 2nd tab
    await dispatch(actions.selectLocation(initialLocation("bar.js")));

    // 1st tab
    await dispatch(actions.selectLocation(initialLocation("baz.js")));

    // 3rd tab is reselected
    await dispatch(actions.selectLocation(initialLocation("foo.js")));
    await dispatch(actions.closeTab(bazSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("foo.js");
    expect(getSourceTabs(getState())).toHaveLength(2);
  });

  it("should not select new sources that lack a URL", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    const source = makeSource("foo");
    source.url = "";
    await dispatch(actions.newSource(source));

    expect(getSourceCount(getState())).toEqual(1);
    const selectedLocation = getSelectedLocation(getState());
    expect(selectedLocation).toEqual(undefined);
  });

  it("sets and clears selected location correctly", () => {
    const { dispatch, getState } = createStore(sourceThreadClient);
    const source = makeSource("testSource");
    const location = ({ test: "testLocation" }: any);

    // set value
    dispatch(actions.setSelectedLocation(source, location));
    expect(getSelectedLocation(getState())).toEqual({
      sourceId: source.id,
      ...location
    });

    // clear value
    dispatch(actions.clearSelectedLocation());
    expect(getSelectedLocation(getState())).toEqual(null);
  });

  it("sets and clears pending selected location correctly", () => {
    const { dispatch, getState } = createStore(sourceThreadClient);
    const url = "testURL";
    const options = { location: { line: "testLine" } };

    // set value
    dispatch(actions.setPendingSelectedLocation(url, options));
    const setResult = getState().sources.pendingSelectedLocation;
    expect(setResult).toEqual({
      url,
      line: options.location.line
    });

    // clear value
    dispatch(actions.clearSelectedLocation());
    const clearResult = getState().sources.pendingSelectedLocation;
    expect(clearResult).toEqual({ url: "" });
  });

  it("should keep the generated the viewing context", async () => {
    const store = createStore(sourceThreadClient);
    const { dispatch, getState } = store;
    const baseSource = makeSource("base.js");
    await dispatch(actions.newSource(baseSource));

    await dispatch(
      actions.selectLocation({ sourceId: baseSource.id, line: 1 })
    );

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe(baseSource.id);
    await waitForState(store, state => getSymbols(state, baseSource));
  });

  it("should keep the original the viewing context", async () => {
    const { dispatch, getState } = createStore(
      sourceThreadClient,
      {},
      {
        getOriginalLocation: async location => ({ ...location, line: 12 }),
        getOriginalLocations: async items => items,
        getGeneratedLocation: async location => ({ ...location, line: 12 }),
        getOriginalSourceText: async () => ({ source: "" }),
        getGeneratedRangesForOriginal: async () => []
      }
    );

    const baseSource = makeSource("base.js");
    await dispatch(actions.newSource(baseSource));

    const originalBaseSource = makeOriginalSource("base.js");
    await dispatch(actions.newSource(originalBaseSource));

    await dispatch(actions.selectSource(originalBaseSource.id));

    const fooSource = makeSource("foo.js");
    await dispatch(actions.newSource(fooSource));
    await dispatch(actions.selectLocation({ sourceId: fooSource.id, line: 1 }));

    const selected = getSelectedLocation(getState());
    expect(selected && selected.line).toBe(12);
  });

  it("should change the original the viewing context", async () => {
    const { dispatch, getState } = createStore(
      sourceThreadClient,
      {},
      {
        getOriginalLocation: async location => ({ ...location, line: 12 }),
        getOriginalLocations: async items => items,
        getGeneratedRangesForOriginal: async () => [],
        getOriginalSourceText: async () => ({ source: "" })
      }
    );

    await dispatch(actions.newSource(makeSource("base.js")));
    const baseSource = makeOriginalSource("base.js");

    await dispatch(actions.newSource(baseSource));
    await dispatch(actions.selectSource(baseSource.id));

    await dispatch(
      actions.selectSpecificLocation({
        sourceId: baseSource.id,
        line: 1
      })
    );

    const selected = getSelectedLocation(getState());
    expect(selected && selected.line).toBe(1);
  });

  describe("selectSourceURL", () => {
    it("should automatically select a pending source", async () => {
      const { dispatch, getState } = createStore(sourceThreadClient);
      const baseSource = makeSource("base.js");
      await dispatch(actions.selectSourceURL(baseSource.url));

      expect(getSelectedSource(getState())).toBe(undefined);
      await dispatch(actions.newSource(baseSource));

      const selected = getSelectedSource(getState());
      expect(selected && selected.url).toBe(baseSource.url);
    });
  });
});
