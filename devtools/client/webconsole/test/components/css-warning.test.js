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
const { setupStore } = require("devtools/client/webconsole/test/helpers");

// Components under test.
const CSSWarning = require("devtools/client/webconsole/components/message-types/CSSWarning");
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("devtools/client/webconsole/constants");

// Test fakes.
const { stubPreparedMessages } = require("devtools/client/webconsole/test/fixtures/stubs/index");
const serviceContainer = require("devtools/client/webconsole/test/fixtures/serviceContainer");

describe("CSSWarning component:", () => {
  it("renders", () => {
    const message = stubPreparedMessages.get(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );
    const wrapper = render(CSSWarning({
      message,
      serviceContainer,
      timestampsVisible: true,
    }));
    const { timestampString } = require("devtools/client/webconsole/webconsole-l10n");

    expect(wrapper.find(".timestamp").text()).toBe(timestampString(message.timeStamp));

    expect(wrapper.find(".message-body").text())
      .toBe("Unknown property ‘such-unknown-property’.  Declaration dropped.");

    // There shouldn't be a matched elements label rendered by default.
    const elementLabel = wrapper.find(`.elements-label`);
    expect(elementLabel.length).toBe(0);

    // There should be a location.
    const locationLink = wrapper.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test-css-message.html:3:25");
  });

  it("closes an open message when the collapse button is clicked", () => {
    const store = setupStore();
    store.dispatch = sinon.spy();
    const message = stubPreparedMessages.get(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );

    const wrapper = mount(Provider({store},
      CSSWarning({
        message,
        open: true,
        dispatch: store.dispatch,
        serviceContainer,
      })
    ));

    wrapper.find(".collapse-button[aria-expanded='true']").simulate("click");

    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_CLOSE,
    });
  });

  it("opens a closed message when the collapse button is clicked", () => {
    const store = setupStore();
    store.dispatch = sinon.spy();
    const message = stubPreparedMessages.get(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );

    const wrapper = mount(Provider({store},
      CSSWarning({
        message,
        open: false,
        payload: {}, // fake the existence of a payload to test just MESSAGE_OPEN action
        dispatch: store.dispatch,
        serviceContainer,
      })
    ));

    wrapper.find(".collapse-button[aria-expanded='false']").simulate("click");

    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_OPEN,
    });
  });
});
