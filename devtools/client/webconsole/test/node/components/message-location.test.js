/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { mount } = require("enzyme");
const sinon = require("sinon");
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");

let {
  MessageContainer,
} = require("resource://devtools/client/webconsole/components/Output/MessageContainer.js");
MessageContainer = createFactory(MessageContainer);

// Test fakes.
const {
  stubPreparedMessages,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");

describe("Message - location element", () => {
  it("Calls onViewSourceInDebugger when clicked", () => {
    const onViewSourceInDebugger = sinon.spy();
    const onViewSource = sinon.spy();

    const message = stubPreparedMessages.get("console.log('foobar', 'test')");
    const wrapper = mount(
      MessageContainer({
        getMessage: () => message,
        serviceContainer: Object.assign({}, serviceContainer, {
          onViewSourceInDebugger,
          onViewSource,
        }),
      })
    );

    // There should be the location
    const locationLink = wrapper.find(`.message-location a`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test-console-api.html:1:35");

    locationLink.simulate("click");

    expect(onViewSourceInDebugger.calledOnce).toBe(true);
    expect(onViewSource.notCalled).toBe(true);
  });

  it("Calls onViewSource when clicked and onViewSourceInDebugger undefined", () => {
    const onViewSource = sinon.spy();

    const message = stubPreparedMessages.get("console.log('foobar', 'test')");

    const wrapper = mount(
      MessageContainer({
        getMessage: () => message,
        serviceContainer: Object.assign({}, serviceContainer, {
          onViewSource,
          onViewSourceInDebugger: undefined,
        }),
      })
    );

    // There should be the location
    const locationLink = wrapper.find(`.message-location a`);

    locationLink.simulate("click");
    expect(onViewSource.calledOnce).toBe(true);
  });
});
