/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { getRepeatId } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");

const expect = require("expect");

describe("getRepeatId:", () => {
  it("returns same repeatId for duplicate values", () => {
    const baseMessage = stubPreparedMessages.get("console.log('foobar', 'test')");

    // Repeat ID must be the same even if the timestamp is different.
    const message1 = Object.assign({}, baseMessage, {"timeStamp": 1});
    const message2 = Object.assign({}, baseMessage, {"timeStamp": 2});

    expect(getRepeatId(message1)).toEqual(getRepeatId(message2));
  });

  it("returns different repeatIds for different values", () => {
    const message1 = stubPreparedMessages.get("console.log('foobar', 'test')");
    const message2 = Object.assign({}, message1, {
      "parameters": ["funny", "monkey"]
    });
    expect(getRepeatId(message1)).toNotEqual(getRepeatId(message2));
  });

  it("returns different repeatIds for different severities", () => {
    const message1 = stubPreparedMessages.get("console.log('foobar', 'test')");
    const message2 = Object.assign({}, message1, {"level": "error"});
    expect(getRepeatId(message1)).toNotEqual(getRepeatId(message2));
  });

  it("handles falsy values distinctly", () => {
    const messageNaN = stubPreparedMessages.get("console.log(NaN)");
    const messageUnd = stubPreparedMessages.get("console.log(undefined)");
    const messageNul = stubPreparedMessages.get("console.log(null)");

    const repeatIds = new Set([
      getRepeatId(messageNaN),
      getRepeatId(messageUnd),
      getRepeatId(messageNul)]
    );
    expect(repeatIds.size).toEqual(3);
  });
});
