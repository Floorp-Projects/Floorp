/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

jest.mock("devtools/client/shared/telemetry", () => {
  function MockTelemetry() {}
  MockTelemetry.prototype.recordEvent = jest.fn();

  return MockTelemetry;
});

const Telemetry = require("devtools/client/shared/telemetry");

import { recordEvent } from "../telemetry";

const telemetry = new Telemetry();

describe("telemetry.recordEvent()", () => {
  it("Receives the correct telemetry information", () => {
    recordEvent("foo", { bar: 1 });

    expect(telemetry.recordEvent).toHaveBeenCalledWith(
      "foo",
      "debugger",
      null,
      {
        // eslint-disable-next-line camelcase
        session_id: -1,
        bar: 1,
      }
    );
  });
});
