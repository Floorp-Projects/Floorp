/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var test: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import update, {
  createQuickOpenState,
  getQuickOpenEnabled,
  getQuickOpenQuery,
  getQuickOpenType,
} from "../quick-open";
import {
  setQuickOpenQuery,
  openQuickOpen,
  closeQuickOpen,
} from "../../actions/quick-open";

describe("quickOpen reducer", () => {
  test("initial state", () => {
    const state = update(undefined, ({ type: "FAKE" }: any));
    expect(getQuickOpenQuery({ quickOpen: state })).toEqual("");
    expect(getQuickOpenType({ quickOpen: state })).toEqual("sources");
  });

  test("opens the quickOpen modal", () => {
    const state = update(createQuickOpenState(), openQuickOpen());
    expect(getQuickOpenEnabled({ quickOpen: state })).toEqual(true);
  });

  test("closes the quickOpen modal", () => {
    let state = update(createQuickOpenState(), openQuickOpen());
    expect(getQuickOpenEnabled({ quickOpen: state })).toEqual(true);
    state = update(createQuickOpenState(), closeQuickOpen());
    expect(getQuickOpenEnabled({ quickOpen: state })).toEqual(false);
  });

  test("leaves query alone on open if not provided", () => {
    const state = update(createQuickOpenState(), openQuickOpen());
    expect(getQuickOpenQuery({ quickOpen: state })).toEqual("");
    expect(getQuickOpenType({ quickOpen: state })).toEqual("sources");
  });

  test("set query on open if provided", () => {
    const state = update(createQuickOpenState(), openQuickOpen("@"));
    expect(getQuickOpenQuery({ quickOpen: state })).toEqual("@");
    expect(getQuickOpenType({ quickOpen: state })).toEqual("functions");
  });

  test("clear query on close", () => {
    const state = update(createQuickOpenState(), closeQuickOpen());
    expect(getQuickOpenQuery({ quickOpen: state })).toEqual("");
    expect(getQuickOpenType({ quickOpen: state })).toEqual("sources");
  });

  test("sets the query to the provided string", () => {
    const state = update(createQuickOpenState(), setQuickOpenQuery("test"));
    expect(getQuickOpenQuery({ quickOpen: state })).toEqual("test");
    expect(getQuickOpenType({ quickOpen: state })).toEqual("sources");
  });
});
