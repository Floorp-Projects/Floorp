/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { getRepeatId } = require("devtools/client/webconsole/new-console-output/utils/messages");
const stubConsoleMessages = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const expect = require("expect");

describe("getRepeatId:", () => {
  it("returns same repeatId for duplicate values", () => {
    const message1 = stubConsoleMessages.get("console.log('foobar', 'test')");
    const message2 = message1.set("repeat", 3);
    expect(getRepeatId(message1)).toEqual(getRepeatId(message2));
  });

  it("returns different repeatIds for different values", () => {
    const message1 = stubConsoleMessages.get("console.log('foobar', 'test')");
    const message2 = message1.set("parameters", ["funny", "monkey"]);
    expect(getRepeatId(message1)).toNotEqual(getRepeatId(message2));
  });

  it("returns different repeatIds for different severities", () => {
    const message1 = stubConsoleMessages.get("console.log('foobar', 'test')");
    const message2 = message1.set("level", "error");
    expect(getRepeatId(message1)).toNotEqual(getRepeatId(message2));
  });

  it("handles falsy values distinctly", () => {
    const messageNaN = stubConsoleMessages.get("console.log(NaN)");
    const messageUnd = stubConsoleMessages.get("console.log(undefined)");
    const messageNul = stubConsoleMessages.get("console.log(null)");

    const repeatIds = new Set([
      getRepeatId(messageNaN),
      getRepeatId(messageUnd),
      getRepeatId(messageNul)]
    );
    expect(repeatIds.size).toEqual(3);
  });
});
