/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  areMessagesSimilar,
} = require("resource://devtools/client/webconsole/utils/messages.js");
const {
  stubPreparedMessages,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");

const expect = require("expect");

describe("areMessagesSimilar:", () => {
  it("returns true for duplicated messages", () => {
    const baseMessage = stubPreparedMessages.get(
      "console.log('foobar', 'test')"
    );

    // Repeat ID must be the same even if the timestamp is different.
    const message1 = Object.assign({}, baseMessage, { timeStamp: 1 });
    const message2 = Object.assign({}, baseMessage, { timeStamp: 2 });

    expect(areMessagesSimilar(message1, message2)).toEqual(true);
  });

  it("returns false for different messages", () => {
    const message1 = stubPreparedMessages.get("console.log('foobar', 'test')");
    const message2 = Object.assign({}, message1, {
      parameters: ["funny", "monkey"],
    });
    expect(areMessagesSimilar(message1, message2)).toEqual(false);
  });

  it("returns false for messages with different severities", () => {
    const message1 = stubPreparedMessages.get("console.log('foobar', 'test')");
    const message2 = Object.assign({}, message1, { level: "error" });
    expect(areMessagesSimilar(message1, message2)).toEqual(false);
  });

  it("return false for messages with different falsy values", () => {
    const messageNaN = stubPreparedMessages.get("console.log(NaN)");
    const messageUnd = stubPreparedMessages.get("console.log(undefined)");
    const messageNul = stubPreparedMessages.get("console.log(null)");

    expect(areMessagesSimilar(messageNaN, messageUnd)).toEqual(false);
    expect(areMessagesSimilar(messageUnd, messageNul)).toEqual(false);
    expect(areMessagesSimilar(messageNul, messageNaN)).toEqual(false);
  });
});
