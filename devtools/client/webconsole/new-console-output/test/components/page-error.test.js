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
    const wrapper = render(PageError({
      message,
      serviceContainer,
      timestampsVisible: true,
    }));
    const { timestampString } = require("devtools/client/webconsole/webconsole-l10n");

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
    expect(locationLink.text()).toBe("test-console-api.html:3:5");
  });

  it("does not have a timestamp when timestampsVisible prop is falsy", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const wrapper = render(PageError({
      message,
      serviceContainer,
      timestampsVisible: false,
    }));

    expect(wrapper.find(".timestamp").length).toBe(0);
  });

  it("renders an error with a longString exception message", () => {
    const message = stubPreparedMessages.get("TypeError longString message");
    const wrapper = render(PageError({ message, serviceContainer }));

    const text = wrapper.find(".message-body").text();
    expect(text.startsWith("Error: Long error Long error")).toBe(true);
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

    // There should be five stacktrace items.
    const frameLinks = wrapper.find(`.stack-trace span.frame-link`);
    expect(frameLinks.length).toBe(5);
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
    let wrapper = render(PageError({
      message: Object.assign({}, message, {indent}),
      serviceContainer
    }));
    expect(wrapper.find(".indent").prop("style").width)
        .toBe(`${indent * INDENT_WIDTH}px`);

    wrapper = render(PageError({ message, serviceContainer}));
    expect(wrapper.find(".indent").prop("style").width).toBe(`0`);
  });

  it("has empty error notes", () => {
    const message = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    let wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");

    expect(notes.length).toBe(0);
  });

  it("can show an error note", () => {
    const origMessage = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const message = Object.assign({}, origMessage, {
      "notes": [{
        "messageBody": "test note",
        "frame": {
          "source": "http://example.com/test.js",
          "line": 2,
          "column": 6
        }
      }]
    });

    let wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(1);

    const note = notes.eq(0);
    expect(note.find(".message-body").text())
      .toBe("note: test note");

    // There should be the location.
    const locationLink = note.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test.js:2:6");
  });

  it("can show multiple error notes", () => {
    const origMessage = stubPreparedMessages.get("ReferenceError: asdf is not defined");
    const message = Object.assign({}, origMessage, {
      "notes": [{
        "messageBody": "test note 1",
        "frame": {
          "source": "http://example.com/test1.js",
          "line": 2,
          "column": 6
        }
      },
      {
        "messageBody": "test note 2",
        "frame": {
          "source": "http://example.com/test2.js",
          "line": 10,
          "column": 18
        }
      },
      {
        "messageBody": "test note 3",
        "frame": {
          "source": "http://example.com/test3.js",
          "line": 9,
          "column": 4
        }
      }]
    });

    let wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(3);

    const note1 = notes.eq(0);
    expect(note1.find(".message-body").text())
      .toBe("note: test note 1");

    const locationLink1 = note1.find(`.message-location`);
    expect(locationLink1.length).toBe(1);
    expect(locationLink1.text()).toBe("test1.js:2:6");

    const note2 = notes.eq(1);
    expect(note2.find(".message-body").text())
      .toBe("note: test note 2");

    const locationLink2 = note2.find(`.message-location`);
    expect(locationLink2.length).toBe(1);
    expect(locationLink2.text()).toBe("test2.js:10:18");

    const note3 = notes.eq(2);
    expect(note3.find(".message-body").text())
      .toBe("note: test note 3");

    const locationLink3 = note3.find(`.message-location`);
    expect(locationLink3.length).toBe(1);
    expect(locationLink3.text()).toBe("test3.js:9:4");
  });

  it("displays error notes", () => {
    const message = stubPreparedMessages.get("SyntaxError: redeclaration of let a");

    let wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(1);

    const note = notes.eq(0);
    expect(note.find(".message-body").text())
      .toBe("note: Previously declared at line 2, column 6");

    // There should be the location.
    const locationLink = note.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test-console-api.html:2:6");
  });
});
