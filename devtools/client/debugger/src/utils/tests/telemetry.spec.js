/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { recordEvent } from "../telemetry";

describe("telemetry.recordEvent()", () => {
  it("Receives the correct telemetry information", () => {
    recordEvent("foo", { bar: 1 });

    expect(window.dbg._telemetry.events.foo).toStrictEqual([{ bar: 1 }]);
  });
});
