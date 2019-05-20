/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource,
} from "../../utils/test-head";
const { getSelectedSource, getSourceTabs } = selectors;

import { sourceThreadClient as threadClient } from "./helpers/threadClient.js";

describe("closing tabs", () => {
  it("closing a tab", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    const fooSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    dispatch(actions.closeTab(cx, fooSource));

    expect(getSelectedSource(getState())).toBe(undefined);
    expect(getSourceTabs(getState())).toHaveLength(0);
  });

  it("closing the inactive tab", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    const fooSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    await dispatch(actions.selectLocation(cx, { sourceId: "bar.js", line: 1 }));
    dispatch(actions.closeTab(cx, fooSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bar.js");
    expect(getSourceTabs(getState())).toHaveLength(1);
  });

  it("closing the only tab", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    const fooSource = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    dispatch(actions.closeTab(cx, fooSource));

    expect(getSelectedSource(getState())).toBe(undefined);
    expect(getSourceTabs(getState())).toHaveLength(0);
  });

  it("closing the active tab", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    const barSource = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    await dispatch(actions.selectLocation(cx, { sourceId: "bar.js", line: 1 }));
    await dispatch(actions.closeTab(cx, barSource));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("foo.js");
    expect(getSourceTabs(getState())).toHaveLength(1);
  });

  it("closing many inactive tabs", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bazz.js")));
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    await dispatch(actions.selectLocation(cx, { sourceId: "bar.js", line: 1 }));
    await dispatch(
      actions.selectLocation(cx, { sourceId: "bazz.js", line: 1 })
    );

    const tabs = [
      "http://localhost:8000/examples/foo.js",
      "http://localhost:8000/examples/bar.js",
    ];
    dispatch(actions.closeTabs(cx, tabs));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("bazz.js");
    expect(getSourceTabs(getState())).toHaveLength(1);
  });

  it("closing many tabs including the active tab", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bazz.js")));
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    await dispatch(actions.selectLocation(cx, { sourceId: "bar.js", line: 1 }));
    await dispatch(
      actions.selectLocation(cx, { sourceId: "bazz.js", line: 1 })
    );
    const tabs = [
      "http://localhost:8000/examples/bar.js",
      "http://localhost:8000/examples/bazz.js",
    ];
    await dispatch(actions.closeTabs(cx, tabs));

    const selected = getSelectedSource(getState());
    expect(selected && selected.id).toBe("foo.js");
    expect(getSourceTabs(getState())).toHaveLength(1);
  });

  it("closing all the tabs", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.selectLocation(cx, { sourceId: "foo.js", line: 1 }));
    await dispatch(actions.selectLocation(cx, { sourceId: "bar.js", line: 1 }));
    await dispatch(
      actions.closeTabs(cx, [
        "http://localhost:8000/examples/foo.js",
        "http://localhost:8000/examples/bar.js",
      ])
    );

    expect(getSelectedSource(getState())).toBe(undefined);
    expect(getSourceTabs(getState())).toHaveLength(0);
  });
});
