/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

jest.useFakeTimers();
const networkRequest = require("../network-request");

describe("network request", () => {
  beforeEach(() => {
    global.fetch = jest.fn();
  });

  it("successful fetch", async () => {
    global.fetch.mockImplementation(async () => ({
      status: 200,
      headers: { get: () => "application/json" },
      text: async () => "Yay",
    }));
    const res = await networkRequest("foo");
    expect(res).toEqual({ content: "Yay" });
  });

  it("wasm successful fetch", async () => {
    global.fetch.mockImplementation(async () => ({
      status: 200,
      headers: { get: () => "application/wasm" },
      arrayBuffer: async () => "Yay",
    }));
    const res = await networkRequest("foo");
    expect(res).toEqual({ content: "Yay", isDwarf: true });
  });

  it("failed fetch", async () => {
    global.fetch.mockImplementation(async () => ({
      status: 400,
      headers: { get: () => "application/json" },
      text: async () => "Sad",
    }));

    try {
      await networkRequest("foo");
    } catch (e) {
      expect(e.message).toEqual("failed to request foo");
    }
  });

  it("timed out fetch", async () => {
    global.fetch.mockImplementation(async () => {});

    try {
      await networkRequest("foo");
    } catch (e) {
      expect(e.message).toEqual("Connect timeout error");
    }

    jest.runAllTimers();
  });
});
