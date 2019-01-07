/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import update, { initialSourcesState } from "../sources";
import { foobar } from "../../test/fixtures";
import type { Source } from "../../types";

const fakeSources = foobar.sources.sources;

describe("sources reducer", () => {
  it("should work", () => {
    let state = initialSourcesState();
    state = update(state, {
      type: "ADD_SOURCE",
      // coercing to a Source for the purpose of this test
      source: ((fakeSources.fooSourceActor: any): Source)
    });
    expect(Object.keys(state.sources)).toHaveLength(1);
  });
});
