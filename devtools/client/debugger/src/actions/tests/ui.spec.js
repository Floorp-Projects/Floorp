/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createStore,
  selectors,
  actions,
  makeSource,
} from "../../utils/test-head";
import { createLocation } from "../../utils/location";
import { mockCommandClient } from "./helpers/mockCommandClient";

const {
  getActiveSearch,
  getFrameworkGroupingState,
  getPaneCollapse,
  getHighlightedLineRangeForSelectedSource,
} = selectors;

describe("ui", () => {
  it("should toggle the visible state of file search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("file"));
    expect(getActiveSearch(getState())).toBe("file");
  });

  it("should close file search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("file"));
    dispatch(actions.closeActiveSearch());
    expect(getActiveSearch(getState())).toBe(null);
  });

  it("should toggle the collapse state of a pane", () => {
    const { dispatch, getState } = createStore();
    expect(getPaneCollapse(getState(), "start")).toBe(false);
    dispatch(actions.togglePaneCollapse("start", true));
    expect(getPaneCollapse(getState(), "start")).toBe(true);
  });

  it("should toggle the collapsed state of frameworks in the callstack", () => {
    const { dispatch, getState } = createStore();
    const currentState = getFrameworkGroupingState(getState());
    dispatch(actions.toggleFrameworkGrouping(!currentState));
    expect(getFrameworkGroupingState(getState())).toBe(!currentState);
  });

  it("should highlight lines", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    const base = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      base.id
    );
    //await dispatch(actions.selectSource(base, sourceActor));
    const location = createLocation({
      source: base,
      line: 1,
      sourceActor,
    });
    await dispatch(actions.selectLocation(location));

    const range = { start: 3, end: 5, sourceId: base.id };
    dispatch(actions.highlightLineRange(range));
    expect(getHighlightedLineRangeForSelectedSource(getState())).toEqual(range);
  });

  it("should clear highlight lines", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    const base = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      base.id
    );
    await dispatch(actions.selectSource(base, sourceActor));
    const range = { start: 3, end: 5, sourceId: "2" };
    dispatch(actions.highlightLineRange(range));
    dispatch(actions.clearHighlightLineRange());
    expect(getHighlightedLineRangeForSelectedSource(getState())).toEqual(null);
  });
});
