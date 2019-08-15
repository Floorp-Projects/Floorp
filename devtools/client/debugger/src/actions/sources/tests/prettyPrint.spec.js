/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource,
} from "../../../utils/test-head";
import { createPrettySource } from "../prettyPrint";
import { sourceThreadFront } from "../../tests/helpers/threadFront.js";
import { isFulfilled } from "../../../utils/async-value";

describe("sources - pretty print", () => {
  it("returns a pretty source for a minified file", async () => {
    const { dispatch, getState, cx } = createStore(sourceThreadFront);

    const url = "base.js";
    const source = await dispatch(actions.newGeneratedSource(makeSource(url)));
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(createPrettySource(cx, source.id));

    const prettyURL = `${source.url}:formatted`;
    const pretty = selectors.getSourceByURL(getState(), prettyURL);
    const content = pretty
      ? selectors.getSourceContent(getState(), pretty.id)
      : null;
    expect(pretty && pretty.url.includes(prettyURL)).toEqual(true);
    expect(pretty).toMatchSnapshot();

    expect(
      content &&
        isFulfilled(content) &&
        content.value.type === "text" &&
        content.value.contentType
    ).toEqual("text/javascript");
    expect(content).toMatchSnapshot();
  });

  it("should create a source when first toggling pretty print", async () => {
    const { dispatch, getState, cx } = createStore(sourceThreadFront);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foobar.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.togglePrettyPrint(cx, source.id));
    expect(selectors.getSourceCount(getState())).toEqual(2);
  });

  it("should not make a second source when toggling pretty print", async () => {
    const { dispatch, getState, cx } = createStore(sourceThreadFront);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foobar.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.togglePrettyPrint(cx, source.id));
    expect(selectors.getSourceCount(getState())).toEqual(2);
    await dispatch(actions.togglePrettyPrint(cx, source.id));
    expect(selectors.getSourceCount(getState())).toEqual(2);
  });
});
