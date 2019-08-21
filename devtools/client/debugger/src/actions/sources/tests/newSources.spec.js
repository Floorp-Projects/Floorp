/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource,
  makeSourceURL,
  waitForState,
} from "../../../utils/test-head";
const {
  getSource,
  getSourceCount,
  getSelectedSource,
  getSourceByURL,
} = selectors;
import sourceQueue from "../../../utils/source-queue";
import { generatedToOriginalId } from "devtools-source-map";

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

  it("should not add multiple identical sources", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);

    await dispatch(actions.newGeneratedSource(makeSource("base.js")));
    await dispatch(actions.newGeneratedSource(makeSource("base.js")));

    expect(getSourceCount(getState())).toEqual(1);
  });

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

  it("should add original sources", async () => {
    const { dispatch, getState } = createStore(
      mockCommandClient,
      {},
      {
        getOriginalURLs: async source => [
          {
            id: generatedToOriginalId(source.id, "magic.js"),
            url: "magic.js",
          },
        ],
        getOriginalLocations: async items => items,
      }
    );

    await dispatch(
      actions.newGeneratedSource(
        makeSource("base.js", { sourceMapURL: "base.js.map" })
      )
    );
    const magic = getSourceByURL(getState(), "magic.js");
    expect(magic && magic.url).toEqual("magic.js");
  });

  // eslint-disable-next-line
  it("should not attempt to fetch original sources if it's missing a source map url", async () => {
    const getOriginalURLs = jest.fn();
    const { dispatch } = createStore(
      mockCommandClient,
      {},
      {
        getOriginalURLs,
        getOriginalLocations: async items => items,
      }
    );

    await dispatch(actions.newGeneratedSource(makeSource("base.js")));
    expect(getOriginalURLs).not.toHaveBeenCalled();
  });

  // eslint-disable-next-line
  it("should process new sources immediately, without waiting for source maps to be fetched first", async () => {
    const { dispatch, getState } = createStore(
      mockCommandClient,
      {},
      {
        getOriginalURLs: async () => new Promise(_ => {}),
        getOriginalLocations: async items => items,
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

  // eslint-disable-next-line
  it("shouldn't let one slow loading source map delay all the other source maps", async () => {
    const dbg = createStore(
      mockCommandClient,
      {},
      {
        getOriginalURLs: async source => {
          if (source.id == "foo.js") {
            // simulate a hang loading foo.js.map
            return new Promise(_ => {});
          }
          const url = source.id.replace(".js", ".cljs");
          return [
            {
              id: generatedToOriginalId(source.id, url),
              url: url,
            },
          ];
        },
        getOriginalLocations: async items => items,
        getGeneratedLocation: location => location,
      }
    );
    const { dispatch, getState } = dbg;
    await dispatch(
      actions.newGeneratedSources([
        makeSource("foo.js", { sourceMapURL: "foo.js.map" }),
        makeSource("bar.js", { sourceMapURL: "bar.js.map" }),
        makeSource("bazz.js", { sourceMapURL: "bazz.js.map" }),
      ])
    );
    await sourceQueue.flush();
    await waitForState(dbg, state => getSourceCount(state) == 5);
    expect(getSourceCount(getState())).toEqual(5);
    const barCljs = getSourceByURL(getState(), "bar.cljs");
    expect(barCljs && barCljs.url).toEqual("bar.cljs");
    const bazzCljs = getSourceByURL(getState(), "bazz.cljs");
    expect(bazzCljs && bazzCljs.url).toEqual("bazz.cljs");
  });

  describe("sources - sources with querystrings", () => {
    it(`should find two sources when same source with
      querystring`, async () => {
      const { getSourcesUrlsInSources } = selectors;
      const { dispatch, getState } = createStore(mockCommandClient);
      await dispatch(actions.newGeneratedSource(makeSource("base.js?v=1")));
      await dispatch(actions.newGeneratedSource(makeSource("base.js?v=2")));
      await dispatch(actions.newGeneratedSource(makeSource("diff.js?v=1")));

      const base1 = "http://localhost:8000/examples/base.js?v=1";
      const diff1 = "http://localhost:8000/examples/diff.js?v=1";
      const diff2 = "http://localhost:8000/examples/diff.js?v=1";

      expect(getSourcesUrlsInSources(getState(), base1)).toHaveLength(2);
      expect(getSourcesUrlsInSources(getState(), base1)).toMatchSnapshot();

      expect(getSourcesUrlsInSources(getState(), diff1)).toHaveLength(1);
      await dispatch(actions.newGeneratedSource(makeSource("diff.js?v=2")));
      expect(getSourcesUrlsInSources(getState(), diff2)).toHaveLength(2);
      expect(getSourcesUrlsInSources(getState(), diff1)).toHaveLength(2);
    });
  });
});
