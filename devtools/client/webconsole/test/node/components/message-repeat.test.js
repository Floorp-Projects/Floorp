/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const expect = require("expect");
const { render } = require("enzyme");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const MessageRepeat = createFactory(
  require("resource://devtools/client/webconsole/components/Output/MessageRepeat.js")
);

describe("MessageRepeat component:", () => {
  it("renders repeated value correctly", () => {
    const rendered = render(MessageRepeat({ repeat: 99 }));
    expect(rendered.hasClass("message-repeats")).toBe(true);
    expect(rendered.text()).toBe("99");
  });
});
