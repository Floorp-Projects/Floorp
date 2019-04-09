/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { asyncStoreHelper } from "../asyncStoreHelper";
import { asyncStorage } from "devtools-modules";

function mockAsyncStorage() {
  const store = {};
  jest.spyOn(asyncStorage, "getItem");
  jest.spyOn(asyncStorage, "setItem");

  asyncStorage.getItem.mockImplementation(key => store[key]);
  asyncStorage.setItem.mockImplementation((key, value) => (store[key] = value));
}

describe("asycStoreHelper", () => {
  it("can get and set values", async () => {
    mockAsyncStorage();
    const asyncStore = asyncStoreHelper("root", { a: "_a" });
    asyncStore.a = 3;
    expect(await asyncStore.a).toEqual(3);
  });

  it("supports default values", async () => {
    mockAsyncStorage();
    const asyncStore = asyncStoreHelper("root", { a: ["_a", {}] });
    expect(await asyncStore.a).toEqual({});
  });

  it("undefined default value", async () => {
    mockAsyncStorage();
    const asyncStore = asyncStoreHelper("root", { a: "_a" });
    expect(await asyncStore.a).toEqual(null);
  });

  it("setting an undefined mapping", async () => {
    mockAsyncStorage();
    const asyncStore = asyncStoreHelper("root", { a: "_a" });
    expect(() => {
      asyncStore.b = 3;
    }).toThrow("AsyncStore: b is not defined");
  });
});
