/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  makeSource,
  makeSourceURL,
  makeOriginalSource,
} from "../../../utils/test-head";
const { getSource, getSourceCount, getSelectedSource } = selectors;

import { mockCommandClient } from "../../tests/helpers/mockCommandClient";

describe("sources - new sources", () => {
  it("should add sources to state", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    await dispatch(actions.newGeneratedSource(makeSource("base.js")));
    await dispatch(actions.newGeneratedSource(makeSource("jquery.js")));

    expect(getSourceCount(getState())).toEqual(2);
    const base = getSource(getState(), "base.js");
    const jquery = getSource(getState(), "jquery.js");
    expect(base && base.id).toEqual("base.js");
    expect(jquery && jquery.id).toEqual("jquery.js");
  });

  it("should not add multiple identical generated sources", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);

    const generated = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );

    await dispatch(actions.newOriginalSources([makeOriginalSource(generated)]));
    await dispatch(actions.newOriginalSources([makeOriginalSource(generated)]));

    expect(getSourceCount(getState())).toEqual(2);
  });

  it("should not add multiple identical original sources", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);

    await dispatch(actions.newGeneratedSource(makeSource("base.js")));
    await dispatch(actions.newGeneratedSource(makeSource("base.js")));

    expect(getSourceCount(getState())).toEqual(1);
  });

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

  // eslint-disable-next-line
  it("should not attempt to fetch original sources if it's missing a source map url", async () => {
    const loadSourceMap = jest.fn();
    const { dispatch } = createStore(
      mockCommandClient,
      {},
      {
        loadSourceMap,
        getOriginalLocations: async items => items,
        getOriginalLocation: location => location,
      }
    );

    await dispatch(actions.newGeneratedSource(makeSource("base.js")));
    expect(loadSourceMap).not.toHaveBeenCalled();
  });

  // eslint-disable-next-line
  it("should process new sources immediately, without waiting for source maps to be fetched first", async () => {
    const { dispatch, getState } = createStore(
      mockCommandClient,
      {},
      {
        loadSourceMap: async () => new Promise(_ => {}),
        getOriginalLocations: async items => items,
        getOriginalLocation: location => location,
      }
    );
    await dispatch(
      actions.newGeneratedSource(
        makeSource("base.js", { sourceMapURL: "base.js.map" })
      )
    );
    expect(getSourceCount(getState())).toEqual(1);
    const base = getSource(getState(), "base.js");
    expect(base && base.id).toEqual("base.js");
  });
});
