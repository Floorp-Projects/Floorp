/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  createSourceObject,
  makeSource,
  makeSourceURL,
  waitForState,
  makeOriginalSource,
} from "../../../utils/test-head";
import {
  getSourceCount,
  getSelectedSource,
  getSourceTabs,
  getSelectedLocation,
  getSymbols,
} from "../../../selectors/";
import { createLocation } from "../../../utils/location";

import { mockCommandClient } from "../../tests/helpers/mockCommandClient";

process.on("unhandledRejection", (reason, p) => {});

function initialLocation(sourceId) {
  return createLocation({ source: createSourceObject(sourceId), line: 1 });
}

describe("sources", () => {
  it("should open a tab for the source", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.selectLocation(initialLocation("foo.js")));

    const tabs = getSourceTabs(getState());
    expect(tabs).toHaveLength(1);
    expect(tabs[0].url).toEqual("http://localhost:8000/examples/foo.js");
  });

  it("should keep the selected source when other tab closed", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    const bazSource = await dispatch(
      actions.newGeneratedSource(makeSource("baz.js"))
    );

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
    const { dispatch, getState } = createStore(mockCommandClient);

    await dispatch(
      actions.newGeneratedSource({
        ...makeSource("foo"),
        url: "",
      })
    );

    expect(getSourceCount(getState())).toEqual(1);
    const selectedLocation = getSelectedLocation(getState());
    expect(selectedLocation).toEqual(undefined);
  });

  it("sets and clears selected location correctly", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("testSource"))
    );
    const location = createLocation({ source });

    // set value
    dispatch(actions.setSelectedLocation(location));
    expect(getSelectedLocation(getState())).toEqual({
      ...location,
    });

    // clear value
    dispatch(actions.clearSelectedLocation());
    expect(getSelectedLocation(getState())).toEqual(null);
  });

  it("sets and clears pending selected location correctly", () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    const url = "testURL";
    const options = { line: "testLine", column: "testColumn" };

    // set value
    dispatch(actions.setPendingSelectedLocation(url, options));
    const setResult = getState().sources.pendingSelectedLocation;
    expect(setResult).toEqual({
      url,
      line: options.line,
      column: options.column,
    });

    // clear value
    dispatch(actions.clearSelectedLocation());
    const clearResult = getState().sources.pendingSelectedLocation;
    expect(clearResult).toEqual({ url: "" });
  });

  it("should keep the generated the viewing context", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState } = store;
    const baseSource = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      baseSource.id
    );

    const location = createLocation({
      source: baseSource,
      line: 1,
      sourceActor,
    });
    await dispatch(actions.selectLocation(location));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe(baseSource.id);
    await waitForState(store, state => getSymbols(state, location));
  });

  it("should change the original the viewing context", async () => {
    const { dispatch, getState } = createStore(
      mockCommandClient,
      {},
      {
        getOriginalLocation: async location => ({ ...location, line: 12 }),
        getOriginalLocations: async items => items,
        getGeneratedRangesForOriginal: async () => [],
        getOriginalSourceText: async () => ({ text: "" }),
      }
    );

    const baseGenSource = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );

    const baseSources = await dispatch(
      actions.newOriginalSources([makeOriginalSource(baseGenSource)])
    );
    await dispatch(actions.selectSource(baseSources[0]));

    await dispatch(
      actions.selectSpecificLocation(
        createLocation({
          source: baseSources[0],
          line: 1,
        })
      )
    );

    const selected = getSelectedLocation(getState());
    expect(selected && selected.line).toBe(1);
  });

  describe("selectSourceURL", () => {
    it("should automatically select a pending source", async () => {
      const { dispatch, getState } = createStore(mockCommandClient);
      const baseSourceURL = makeSourceURL("base.js");
      await dispatch(actions.selectSourceURL(baseSourceURL));

      expect(getSelectedSource(getState())).toBe(undefined);
      const baseSource = await dispatch(
        actions.newGeneratedSource(makeSource("base.js"))
      );

      const selected = getSelectedSource(getState());
      expect(selected && selected.url).toBe(baseSource.url);
    });
  });
});
