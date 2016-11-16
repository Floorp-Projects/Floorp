/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render, mount } = require("enzyme");
const sinon = require("sinon");

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("react-redux").Provider);
const { setupStore } = require("devtools/client/webconsole/new-console-output/test/helpers");

// Components under test.
const PageError = require("devtools/client/webconsole/new-console-output/components/message-types/page-error");
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("devtools/client/webconsole/new-console-output/constants");
const { INDENT_WIDTH } = require("devtools/client/webconsole/new-console-output/components/message-indent");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/new-console-output/test/fixtures/serviceContainer");

describe("PageError component:", () => {
  it("renders", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const wrapper = render(PageError({ message, serviceContainer }));
    const L10n = require("devtools/client/webconsole/new-console-output/test/fixtures/L10n");
    const { timestampString } = new L10n();

    expect(wrapper.find(".timestamp").text()).toBe(timestampString(message.timeStamp));

    expect(wrapper.find(".message-body").text())
      .toBe("ReferenceError: asdf is not defined[Learn More]");

    // The stacktrace should be closed by default.
    const frameLinks = wrapper.find(`.stack-trace`);
    expect(frameLinks.length).toBe(0);

    // There should be the location.
    const locationLink = wrapper.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    // @TODO Will likely change. See bug 1307952
    expect(locationLink.text()).toBe("test-tempfile.js:3:5");
  });

  it("displays a [Learn more] link", () => {
    const store = setupStore([]);

    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");

    serviceContainer.openLink = sinon.spy();
    const wrapper = mount(Provider({store},
      PageError({message, serviceContainer})
    ));

    // There should be a [Learn more] link.
    const url =
      "https://developer.mozilla.org/docs/Web/JavaScript/Reference/Errors/Not_defined";
    const learnMore = wrapper.find(".learn-more-link");
    expect(learnMore.length).toBe(1);
    expect(learnMore.prop("title")).toBe(url);

    learnMore.simulate("click");
    let call = serviceContainer.openLink.getCall(0);
    expect(call.args[0]).toEqual(message.exceptionDocURL);
  });

  it("has a stacktrace which can be openned", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const wrapper = render(PageError({ message, serviceContainer, open: true }));

    // There should be a collapse button.
    expect(wrapper.find(".theme-twisty.open").length).toBe(1);

    // There should be three stacktrace items.
    const frameLinks = wrapper.find(`.stack-trace span.frame-link`);
    expect(frameLinks.length).toBe(3);
  });

  it("toggle the stacktrace when the collapse button is clicked", () => {
    const store = setupStore([]);
    store.dispatch = sinon.spy();
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");

    let wrapper = mount(Provider({store},
      PageError({
        message,
        open: true,
        dispatch: store.dispatch,
        serviceContainer,
      })
    ));
    wrapper.find(".theme-twisty.open").simulate("click");
    let call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_CLOSE
    });

    wrapper = mount(Provider({store},
      PageError({
        message,
        open: false,
        dispatch: store.dispatch,
        serviceContainer,
      })
    ));
    wrapper.find(".theme-twisty").simulate("click");
    call = store.dispatch.getCall(1);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_OPEN
    });
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
