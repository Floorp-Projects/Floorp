/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  createSourceObject,
  makeFrame,
  makeSource,
  makeSourceURL,
  waitForState,
  makeOriginalSource,
} from "../../../utils/test-head";
import {
  getSource,
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
  it("should select a source", async () => {
    // Note that we pass an empty client in because the action checks
    // if it exists.
    const store = createStore(mockCommandClient);
    const { dispatch, getState } = store;

    const frame = makeFrame({ id: "1", sourceId: "foo1" });

    const baseSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    await dispatch(
      actions.paused({
        thread: "FakeThread",
        why: { type: "debuggerStatement" },
        frame,
        frames: [frame],
      })
    );

    const cx = selectors.getThreadContext(getState());
    await dispatch(
      actions.selectLocation(
        cx,
        createLocation({ source: baseSource, line: 1, column: 5 })
      )
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
  });

  it("should select next tab on tab closed if no previous tab", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);

    const fooSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.newGeneratedSource(makeSource("baz.js")));

    // 3rd tab
    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));

    // 2nd tab
    await dispatch(actions.selectLocation(cx, initialLocation("bar.js")));

    // 1st tab
    await dispatch(actions.selectLocation(cx, initialLocation("baz.js")));

    // 3rd tab is reselected
    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));

    // closes the 1st tab, which should have no previous tab
    await dispatch(actions.closeTab(cx, fooSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bar.js");
    expect(getSourceTabs(getState())).toHaveLength(2);
  });

  it("should open a tab for the source", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));

    const tabs = getSourceTabs(getState());
    expect(tabs).toHaveLength(1);
    expect(tabs[0].url).toEqual("http://localhost:8000/examples/foo.js");
  });

  it("should select previous tab on tab closed", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));

    const bazSource = await dispatch(
      actions.newGeneratedSource(makeSource("baz.js"))
    );

    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));
    await dispatch(actions.selectLocation(cx, initialLocation("bar.js")));
    await dispatch(actions.selectLocation(cx, initialLocation("baz.js")));
    await dispatch(actions.closeTab(cx, bazSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bar.js");
    expect(getSourceTabs(getState())).toHaveLength(2);
  });

  it("should keep the selected source when other tab closed", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    const bazSource = await dispatch(
      actions.newGeneratedSource(makeSource("baz.js"))
    );

    // 3rd tab
    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));

    // 2nd tab
    await dispatch(actions.selectLocation(cx, initialLocation("bar.js")));

    // 1st tab
    await dispatch(actions.selectLocation(cx, initialLocation("baz.js")));

    // 3rd tab is reselected
    await dispatch(actions.selectLocation(cx, initialLocation("foo.js")));
    await dispatch(actions.closeTab(cx, bazSource));

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
    const { dispatch, getState, cx } = createStore(mockCommandClient);
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("testSource"))
    );
    const location = createLocation({ source });

    // set value
    dispatch(actions.setSelectedLocation(cx, location));
    expect(getSelectedLocation(getState())).toEqual({
      sourceId: source.id,
      ...location,
    });

    // clear value
    dispatch(actions.clearSelectedLocation(cx));
    expect(getSelectedLocation(getState())).toEqual(null);
  });

  it("sets and clears pending selected location correctly", () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);
    const url = "testURL";
    const options = { line: "testLine", column: "testColumn" };

    // set value
    dispatch(actions.setPendingSelectedLocation(cx, url, options));
    const setResult = getState().sources.pendingSelectedLocation;
    expect(setResult).toEqual({
      url,
      line: options.line,
      column: options.column,
    });

    // clear value
    dispatch(actions.clearSelectedLocation(cx));
    const clearResult = getState().sources.pendingSelectedLocation;
    expect(clearResult).toEqual({ url: "" });
  });

  it("should keep the generated the viewing context", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState, cx } = store;
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
    await dispatch(actions.selectLocation(cx, location));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe(baseSource.id);
    await waitForState(store, state => getSymbols(state, location));
  });

  it("should change the original the viewing context", async () => {
    const { dispatch, getState, cx } = createStore(
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
    await dispatch(actions.selectSource(cx, baseSources[0]));

    await dispatch(
      actions.selectSpecificLocation(
        cx,
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
      const { dispatch, getState, cx } = createStore(mockCommandClient);
      const baseSourceURL = makeSourceURL("base.js");
      await dispatch(actions.selectSourceURL(cx, baseSourceURL));

      expect(getSelectedSource(getState())).toBe(undefined);
      const baseSource = await dispatch(
        actions.newGeneratedSource(makeSource("base.js"))
      );

      const selected = getSelectedSource(getState());
      expect(selected && selected.url).toBe(baseSource.url);
    });
  });
});
