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
const { setupStore } = require("devtools/client/webconsole/test/node/helpers");
const { prepareMessage } = require("devtools/client/webconsole/utils/messages");

// Components under test.
const PageError = require("devtools/client/webconsole/components/Output/message-types/PageError");
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("devtools/client/webconsole/constants");
const {
  INDENT_WIDTH,
} = require("devtools/client/webconsole/components/Output/MessageIndent");

// Test fakes.
const {
  stubPackets,
  stubPreparedMessages,
} = require("devtools/client/webconsole/test/node/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/node/fixtures/serviceContainer");

describe("PageError component:", () => {
  it("renders", () => {
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const wrapper = render(
      PageError({
        message,
        serviceContainer,
        timestampsVisible: true,
      })
    );
    const {
      timestampString,
    } = require("devtools/client/webconsole/utils/l10n");

    expect(wrapper.find(".timestamp").text()).toBe(
      timestampString(message.timeStamp)
    );

    expect(wrapper.find(".message-body").text()).toBe(
      "Uncaught ReferenceError: asdf is not defined[Learn More]"
    );

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
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const wrapper = render(
      PageError({
        message,
        serviceContainer,
        timestampsVisible: false,
      })
    );

    expect(wrapper.find(".timestamp").length).toBe(0);
  });

  it("renders an error with a longString exception message", () => {
    const message = stubPreparedMessages.get("TypeError longString message");
    const wrapper = render(PageError({ message, serviceContainer }));

    const text = wrapper.find(".message-body").text();
    expect(text.startsWith("Uncaught Error: Long error Long error")).toBe(true);
  });

  it("renders thrown empty string", () => {
    const message = stubPreparedMessages.get(`throw ""`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught <empty string>");
  });

  it("renders thrown string", () => {
    const message = stubPreparedMessages.get(`throw "tomato"`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught tomato`);
  });

  it("renders thrown boolean", () => {
    const message = stubPreparedMessages.get(`throw false`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught false`);
  });

  it("renders thrown number ", () => {
    const message = stubPreparedMessages.get(`throw 0`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught 0`);
  });

  it("renders thrown null", () => {
    const message = stubPreparedMessages.get(`throw null`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught null`);
  });

  it("renders thrown undefined", () => {
    const message = stubPreparedMessages.get(`throw undefined`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught undefined`);
  });

  it("renders thrown Symbol", () => {
    const message = stubPreparedMessages.get(`throw Symbol`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Symbol(potato)`);
  });

  it("renders thrown object", () => {
    const message = stubPreparedMessages.get(`throw Object`);

    // We need to wrap the PageError in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        PageError({ message, serviceContainer })
      )
    );

    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Object { vegetable: "cucumber" }`);
  });

  it("renders thrown error", () => {
    const message = stubPreparedMessages.get(`throw Error Object`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Error: pumpkin`);
  });

  it("renders thrown Error with custom name", () => {
    const message = stubPreparedMessages.get(
      `throw Error Object with custom name`
    );
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught JuicyError: pineapple`);
  });

  it("renders uncaught rejected Promise with empty string", () => {
    const message = stubPreparedMessages.get(`Promise reject ""`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe("Uncaught <empty string>");
  });

  it("renders uncaught rejected Promise with string", () => {
    const message = stubPreparedMessages.get(`Promise reject "tomato"`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught tomato`);
  });

  it("renders uncaught rejected Promise with boolean", () => {
    const message = stubPreparedMessages.get(`Promise reject false`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught false`);
  });

  it("renders uncaught rejected Promise with number ", () => {
    const message = stubPreparedMessages.get(`Promise reject 0`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught 0`);
  });

  it("renders uncaught rejected Promise with null", () => {
    const message = stubPreparedMessages.get(`Promise reject null`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught null`);
  });

  it("renders uncaught rejected Promise with undefined", () => {
    const message = stubPreparedMessages.get(`Promise reject undefined`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught undefined`);
  });

  it("renders uncaught rejected Promise with Symbol", () => {
    const message = stubPreparedMessages.get(`Promise reject Symbol`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Symbol(potato)`);
  });

  it("renders uncaught rejected Promise with object", () => {
    const message = stubPreparedMessages.get(`Promise reject Object`);
    // We need to wrap the PageError in a Provider in order for the
    // ObjectInspector to work.
    const wrapper = render(
      Provider(
        { store: setupStore() },
        PageError({ message, serviceContainer })
      )
    );
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Object { vegetable: "cucumber" }`);
  });

  it("renders uncaught rejected Promise with error", () => {
    const message = stubPreparedMessages.get(`Promise reject Error Object`);
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught Error: pumpkin`);
  });

  it("renders uncaught rejected Promise with Error with custom name", () => {
    const message = stubPreparedMessages.get(
      `Promise reject Error Object with custom name`
    );
    const wrapper = render(PageError({ message, serviceContainer }));
    const text = wrapper.find(".message-body").text();
    expect(text).toBe(`Uncaught JuicyError: pineapple`);
  });

  it("renders URLs in message as actual, cropped, links", () => {
    // Let's replace the packet data in order to mimick a pageError.
    const packet = stubPackets.get("throw string with URL");

    const evilDomain = `https://evil.com/?`;
    const badDomain = `https://not-so-evil.com/?`;
    const paramLength = 200;
    const longParam = "a".repeat(paramLength);

    const evilURL = `${evilDomain}${longParam}`;
    const badURL = `${badDomain}${longParam}`;

    // We remove the exceptionDocURL to not have the "learn more" link.
    packet.pageError.exceptionDocURL = null;

    const message = prepareMessage(packet, { getNextId: () => "1" });
    const wrapper = render(PageError({ message, serviceContainer }));

    // Keep in sync with `urlCropLimit` in PageError.js.
    const cropLimit = 120;
    const partLength = cropLimit / 2;
    const getCroppedUrl = url =>
      `${url}${"a".repeat(partLength - url.length)}…${"a".repeat(partLength)}`;

    const croppedEvil = getCroppedUrl(evilDomain);
    const croppedbad = getCroppedUrl(badDomain);

    const text = wrapper.find(".message-body").text();
    expect(text).toBe(
      `Uncaught “${croppedEvil}“ is evil and “${croppedbad}“ is not good either`
    );

    // There should be 2 links.
    const links = wrapper.find(".message-body a");
    expect(links.length).toBe(2);

    expect(links.eq(0).attr("href")).toBe(evilURL);
    expect(links.eq(0).attr("title")).toBe(evilURL);

    expect(links.eq(1).attr("href")).toBe(badURL);
    expect(links.eq(1).attr("title")).toBe(badURL);
  });

  it("displays a [Learn more] link", () => {
    const store = setupStore();

    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );

    serviceContainer.openLink = sinon.spy();
    const wrapper = mount(
      Provider(
        { store },
        PageError({
          message,
          serviceContainer,
          dispatch: () => {},
        })
      )
    );

    // There should be a [Learn more] link.
    const url =
      "https://developer.mozilla.org/docs/Web/JavaScript/Reference/Errors/Not_defined";
    const learnMore = wrapper.find(".learn-more-link");
    expect(learnMore.length).toBe(1);
    expect(learnMore.prop("title")).toBe(url);

    learnMore.simulate("click");
    const call = serviceContainer.openLink.getCall(0);
    expect(call.args[0]).toEqual(message.exceptionDocURL);
  });

  // Unskip will happen in Bug 1529548.
  it.skip("has a stacktrace which can be opened", () => {
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const wrapper = render(
      PageError({ message, serviceContainer, open: true })
    );

    // There should be a collapse button.
    expect(wrapper.find(".collapse-button[aria-expanded=true]").length).toBe(1);

    // There should be five stacktrace items.
    const frameLinks = wrapper.find(`.stack-trace span.frame-link`);
    expect(frameLinks.length).toBe(5);
  });

  // Unskip will happen in Bug 1529548.
  it.skip("toggle the stacktrace when the collapse button is clicked", () => {
    const store = setupStore();
    store.dispatch = sinon.spy();
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );

    let wrapper = mount(
      Provider(
        { store },
        PageError({
          message,
          open: true,
          dispatch: store.dispatch,
          serviceContainer,
        })
      )
    );

    wrapper.find(".collapse-button[aria-expanded='true']").simulate("click");
    let call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_CLOSE,
    });

    wrapper = mount(
      Provider(
        { store },
        PageError({
          message,
          open: false,
          dispatch: store.dispatch,
          serviceContainer,
        })
      )
    );
    wrapper.find(".collapse-button[aria-expanded='false']").simulate("click");
    call = store.dispatch.getCall(1);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_OPEN,
    });
  });

  it("has the expected indent", () => {
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const indent = 10;
    let wrapper = render(
      PageError({
        message: Object.assign({}, message, { indent }),
        serviceContainer,
      })
    );
    let indentEl = wrapper.find(".indent");
    expect(indentEl.prop("style").width).toBe(`${indent * INDENT_WIDTH}px`);
    expect(indentEl.prop("data-indent")).toBe(`${indent}`);

    wrapper = render(PageError({ message, serviceContainer }));
    indentEl = wrapper.find(".indent");
    expect(indentEl.prop("style").width).toBe(`0`);
    expect(indentEl.prop("data-indent")).toBe(`0`);
  });

  it("has empty error notes", () => {
    const message = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");

    expect(notes.length).toBe(0);
  });

  it("can show an error note", () => {
    const origMessage = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const message = Object.assign({}, origMessage, {
      notes: [
        {
          messageBody: "test note",
          frame: {
            source: "http://example.com/test.js",
            line: 2,
            column: 6,
          },
        },
      ],
    });

    const wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(1);

    const note = notes.eq(0);
    expect(note.find(".message-body").text()).toBe("note: test note");

    // There should be the location.
    const locationLink = note.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test.js:2:6");
  });

  it("can show multiple error notes", () => {
    const origMessage = stubPreparedMessages.get(
      "ReferenceError: asdf is not defined"
    );
    const message = Object.assign({}, origMessage, {
      notes: [
        {
          messageBody: "test note 1",
          frame: {
            source: "http://example.com/test1.js",
            line: 2,
            column: 6,
          },
        },
        {
          messageBody: "test note 2",
          frame: {
            source: "http://example.com/test2.js",
            line: 10,
            column: 18,
          },
        },
        {
          messageBody: "test note 3",
          frame: {
            source: "http://example.com/test3.js",
            line: 9,
            column: 4,
          },
        },
      ],
    });

    const wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(3);

    const note1 = notes.eq(0);
    expect(note1.find(".message-body").text()).toBe("note: test note 1");

    const locationLink1 = note1.find(`.message-location`);
    expect(locationLink1.length).toBe(1);
    expect(locationLink1.text()).toBe("test1.js:2:6");

    const note2 = notes.eq(1);
    expect(note2.find(".message-body").text()).toBe("note: test note 2");

    const locationLink2 = note2.find(`.message-location`);
    expect(locationLink2.length).toBe(1);
    expect(locationLink2.text()).toBe("test2.js:10:18");

    const note3 = notes.eq(2);
    expect(note3.find(".message-body").text()).toBe("note: test note 3");

    const locationLink3 = note3.find(`.message-location`);
    expect(locationLink3.length).toBe(1);
    expect(locationLink3.text()).toBe("test3.js:9:4");
  });

  it("displays error notes", () => {
    const message = stubPreparedMessages.get(
      "SyntaxError: redeclaration of let a"
    );

    const wrapper = render(PageError({ message, serviceContainer }));

    const notes = wrapper.find(".error-note");
    expect(notes.length).toBe(1);

    const note = notes.eq(0);
    expect(note.find(".message-body").text()).toBe(
      "note: Previously declared at line 2, column 6"
    );

    // There should be the location.
    const locationLink = note.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test-console-api.html:2:6");
  });
});
