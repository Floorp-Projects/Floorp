/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource,
  waitForState
} from "../../../utils/test-head";
const {
  getSource,
  getSourceCount,
  getSelectedSource,
  getSourceByURL
} = selectors;
import sourceQueue from "../../../utils/source-queue";

// eslint-disable-next-line max-len
import { sourceThreadClient as threadClient } from "../../tests/helpers/threadClient.js";

describe("sources - new sources", () => {
  it("should add sources to state", async () => {
    const { dispatch, getState } = createStore(threadClient);
    await dispatch(actions.newSource(makeSource("base.js")));
    await dispatch(actions.newSource(makeSource("jquery.js")));

    expect(getSourceCount(getState())).toEqual(2);
    const base = getSource(getState(), "base.js");
    const jquery = getSource(getState(), "jquery.js");
    expect(base && base.id).toEqual("base.js");
    expect(jquery && jquery.id).toEqual("jquery.js");
  });

  it("should not add multiple identical sources", async () => {
    const { dispatch, getState } = createStore(threadClient);

    await dispatch(actions.newSource(makeSource("base.js")));
    await dispatch(actions.newSource(makeSource("base.js")));

    expect(getSourceCount(getState())).toEqual(1);
  });

  it("should automatically select a pending source", async () => {
    const { dispatch, getState } = createStore(threadClient);
    const baseSource = makeSource("base.js");
    await dispatch(actions.selectSourceURL(baseSource.url));

    expect(getSelectedSource(getState())).toBe(undefined);
    await dispatch(actions.newSource(baseSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.url).toBe(baseSource.url);
  });

  it("should add original sources", async () => {
    const { dispatch, getState } = createStore(
      threadClient,
      {},
      {
        getOriginalURLs: async () => ["magic.js"],
        getOriginalLocations: async items => items
      }
    );

    const baseSource = makeSource("base.js", { sourceMapURL: "base.js.map" });
    await dispatch(actions.newSource(baseSource));
    const magic = getSourceByURL(getState(), "magic.js");
    expect(magic && magic.url).toEqual("magic.js");
  });

  // eslint-disable-next-line
  it("should not attempt to fetch original sources if it's missing a source map url", async () => {
    const getOriginalURLs = jest.fn();
    const { dispatch } = createStore(
      threadClient,
      {},
      {
        getOriginalURLs,
        getOriginalLocations: async items => items
      }
    );

    await dispatch(actions.newSource(makeSource("base.js")));
    expect(getOriginalURLs).not.toHaveBeenCalled();
  });

  // eslint-disable-next-line
  it("should process new sources immediately, without waiting for source maps to be fetched first", async () => {
    const { dispatch, getState } = createStore(
      threadClient,
      {},
      {
        getOriginalURLs: async () => new Promise(_ => {}),
        getOriginalLocations: async items => items
      }
    );
    const baseSource = makeSource("base.js", { sourceMapURL: "base.js.map" });
    await dispatch(actions.newSource(baseSource));
    expect(getSourceCount(getState())).toEqual(1);
    const base = getSource(getState(), "base.js");
    expect(base && base.id).toEqual("base.js");
  });

  // eslint-disable-next-line
  it("shouldn't let one slow loading source map delay all the other source maps", async () => {
    const dbg = createStore(
      threadClient,
      {},
      {
        getOriginalURLs: async source => {
          if (source.id == "foo.js") {
            // simulate a hang loading foo.js.map
            return new Promise(_ => {});
          }

          return [source.id.replace(".js", ".cljs")];
        },
        getOriginalLocations: async items => items,
        getGeneratedLocation: location => location
      }
    );
    const { dispatch, getState } = dbg;
    const fooSource = makeSource("foo.js", { sourceMapURL: "foo.js.map" });
    const barSource = makeSource("bar.js", { sourceMapURL: "bar.js.map" });
    const bazzSource = makeSource("bazz.js", { sourceMapURL: "bazz.js.map" });
    await dispatch(actions.newSources([fooSource, barSource, bazzSource]));
    await sourceQueue.flush();
    await waitForState(dbg, state => getSourceCount(state) == 5);
    expect(getSourceCount(getState())).toEqual(5);
    const barCljs = getSourceByURL(getState(), "bar.cljs");
    expect(barCljs && barCljs.url).toEqual("bar.cljs");
    const bazzCljs = getSourceByURL(getState(), "bazz.cljs");
    expect(bazzCljs && bazzCljs.url).toEqual("bazz.cljs");
  });
});
