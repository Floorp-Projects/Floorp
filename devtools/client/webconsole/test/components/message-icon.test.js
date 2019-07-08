/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { MESSAGE_LEVEL } = require("devtools/client/webconsole/constants");

const expect = require("expect");
const { render } = require("enzyme");
const { createFactory } = require("devtools/client/shared/vendor/react");
const MessageIcon = createFactory(
  require("devtools/client/webconsole/components/Output/MessageIcon")
);

describe("MessageIcon component:", () => {
  it("renders icon based on level", () => {
    const rendered = render(MessageIcon({ level: MESSAGE_LEVEL.ERROR }));
    expect(rendered.hasClass("icon")).toBe(true);
    expect(rendered.attr("title")).toBe("Error");
    expect(rendered.attr("aria-live")).toBe("off");
  });

  it("renders logpoint items", () => {
    const rendered = render(
      MessageIcon({
        level: MESSAGE_LEVEL.LOG,
        type: "logPoint",
      })
    );
    expect(rendered.hasClass("logpoint")).toBe(true);
  });
});
