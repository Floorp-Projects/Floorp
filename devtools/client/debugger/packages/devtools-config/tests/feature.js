/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { isDevelopment, getValue, isEnabled, setConfig } = require("../src/feature");

describe("feature", () => {
  it("isDevelopment", () => {
    setConfig({ development: true });
    expect(isDevelopment()).toEqual(true)
  });

  it("isDevelopment - not defined", () => {
    setConfig({ });
    expect(isDevelopment()).toEqual(true);
  });

  it("getValue - enabled", function() {
    setConfig({ featureA: true });
    expect(getValue("featureA")).toEqual(true);
  });

  it("getValue - disabled", function() {
    setConfig({ featureA: false });
    expect(getValue("featureA")).toEqual(false);
  });

  it("getValue - not present", function() {
    setConfig({});
    expect(getValue("featureA")).toEqual(undefined);
  });

  it("isEnabled - enabled", function() {
    setConfig({ features: { featureA: true }});
    expect(isEnabled("featureA")).toEqual(true);
  });

  it("isEnabled - disabled", function() {
    setConfig({ features: { featureA: false }});
    expect(isEnabled("featureA")).toEqual(false);
  });
});
