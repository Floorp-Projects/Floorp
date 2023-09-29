/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  MESSAGE_LEVEL,
  MESSAGE_TYPE,
} = require("resource://devtools/client/webconsole/constants.js");

const expect = require("expect");
const { render } = require("enzyme");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const MessageIcon = createFactory(
  require("resource://devtools/client/webconsole/components/Output/MessageIcon.js")
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

  it("renders evaluation expression items", () => {
    const rendered = render(
      MessageIcon({
        level: MESSAGE_LEVEL.LOG,
        type: MESSAGE_TYPE.COMMAND,
      })
    );
    expect(rendered.hasClass("icon")).toBe(true);
    expect(rendered.attr("title")).toBe("Evaluated code");
  });

  it("renders evaluation expression result items", () => {
    const rendered = render(
      MessageIcon({
        level: MESSAGE_LEVEL.LOG,
        type: MESSAGE_TYPE.RESULT,
      })
    );
    expect(rendered.hasClass("icon")).toBe(true);
    expect(rendered.attr("title")).toBe("Evaluation result");
  });

  it("renders icon with custom title", () => {
    const expectedTitle = "Rendered with custom title";
    const rendered = render(
      MessageIcon({
        level: MESSAGE_LEVEL.INFO,
        type: "info",
        title: expectedTitle,
      })
    );
    expect(rendered.attr("title")).toBe(expectedTitle);
  });
});
