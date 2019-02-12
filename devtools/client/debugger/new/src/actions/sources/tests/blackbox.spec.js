/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource
} from "../../../utils/test-head";

describe("blackbox", () => {
  it("should blackbox a source", async () => {
    const store = createStore({ blackBox: async () => true });
    const { dispatch, getState } = store;

    const foo1CSR = makeSource("foo1");
    await dispatch(actions.newSource(foo1CSR));
    await dispatch(actions.toggleBlackBox(foo1CSR.source));

    const fooSource = selectors.getSource(getState(), "foo1");

    if (!fooSource) {
      throw new Error("foo should exist");
    }

    const thread = (foo1CSR.sourceActor: any).thread;
    const relativeSources = selectors.getRelativeSourcesForThread(
      getState(),
      thread
    );

    expect(relativeSources[fooSource.id].isBlackBoxed).toEqual(true);
    expect(fooSource.isBlackBoxed).toEqual(true);
  });
});
