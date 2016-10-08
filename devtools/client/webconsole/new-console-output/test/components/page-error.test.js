/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render } = require("enzyme");

// Components under test.
const PageError = require("devtools/client/webconsole/new-console-output/components/message-types/page-error");
const { INDENT_WIDTH } = require("devtools/client/webconsole/new-console-output/components/message-indent");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("PageError component:", () => {
  it("renders", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const wrapper = render(PageError({ message, serviceContainer }));

    expect(wrapper.find(".message-body").text())
      .toBe("ReferenceError: asdf is not defined");

    // The stacktrace should be closed by default.
    const frameLinks = wrapper.find(`.stack-trace`);
    expect(frameLinks.length).toBe(0);

    // There should be the location
    const locationLink = wrapper.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    // @TODO Will likely change. See https://github.com/devtools-html/gecko-dev/issues/285
    expect(locationLink.text()).toBe("test-tempfile.js:3:5");
  });

  it("has a stacktrace which can be openned", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const wrapper = render(PageError({ message, serviceContainer, open: true }));

    // There should be three stacktrace items.
    const frameLinks = wrapper.find(`.stack-trace span.frame-link`);
    expect(frameLinks.length).toBe(3);
  });

  it("has the expected indent", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const indent = 10;
    let wrapper = render(PageError({ message, serviceContainer, indent}));
    expect(wrapper.find(".indent").prop("style").width)
        .toBe(`${indent * INDENT_WIDTH}px`);

    wrapper = render(PageError({ message, serviceContainer}));
    expect(wrapper.find(".indent").prop("style").width).toBe(`0`);
  });
});
