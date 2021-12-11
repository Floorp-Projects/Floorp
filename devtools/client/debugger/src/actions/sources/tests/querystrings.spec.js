/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  makeSource,
} from "../../../utils/test-head";
const { getSourcesUrlsInSources } = selectors;

// eslint-disable-next-line max-len
import { mockCommandClient } from "../../tests/helpers/mockCommandClient";

describe("sources - sources with querystrings", () => {
  it("should find two sources when same source with querystring", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);
    await dispatch(actions.newGeneratedSource(makeSource("base.js?v=1")));
    await dispatch(actions.newGeneratedSource(makeSource("base.js?v=2")));
    await dispatch(actions.newGeneratedSource(makeSource("diff.js?v=1")));

    expect(
      getSourcesUrlsInSources(
        getState(),
        "http://localhost:8000/examples/base.js?v=1"
      )
    ).toHaveLength(2);
    expect(
      getSourcesUrlsInSources(
        getState(),
        "http://localhost:8000/examples/diff.js?v=1"
      )
    ).toHaveLength(1);
  });
});
