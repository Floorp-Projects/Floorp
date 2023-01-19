/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test utils.
const expect = require("expect");
const { render, mount } = require("enzyme");
const sinon = require("sinon");

// React
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const {
  setupStore,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");

// Components under test.
const CSSWarning = require("resource://devtools/client/webconsole/components/Output/message-types/CSSWarning.js");
const {
  MESSAGE_OPEN,
  MESSAGE_CLOSE,
} = require("resource://devtools/client/webconsole/constants.js");

// Test fakes.
const {
  stubPreparedMessages,
} = require("resource://devtools/client/webconsole/test/node/fixtures/stubs/index.js");
const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");

describe("CSSWarning component:", () => {
  it("renders", () => {
    const message = stubPreparedMessages.get(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );
    const wrapper = render(
      CSSWarning({
        message,
        serviceContainer,
        timestampsVisible: true,
      })
    );
    const {
      timestampString,
    } = require("resource://devtools/client/webconsole/utils/l10n.js");

    expect(wrapper.find(".timestamp").text()).toBe(
      timestampString(message.timeStamp)
    );

    expect(wrapper.find(".message-body").text()).toBe(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );

    // There shouldn't be a matched elements label rendered by default.
    const elementLabel = wrapper.find(`.elements-label`);
    expect(elementLabel.length).toBe(0);

    // There should be a location.
    const locationLink = wrapper.find(`.message-location`);
    expect(locationLink.length).toBe(1);
    expect(locationLink.text()).toBe("test-css-message.html:3:27");
  });

  it("closes an open message when the collapse button is clicked", () => {
    const store = setupStore();
    store.dispatch = sinon.spy();
    const message = stubPreparedMessages.get(
      "Unknown property ‘such-unknown-property’.  Declaration dropped."
    );

    const wrapper = mount(
      Provider(
        { store },
        CSSWarning({
          message,
          open: true,
          dispatch: store.dispatch,
          serviceContainer,
        })
      )
    );

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

    const wrapper = mount(
      Provider(
        { store },
        CSSWarning({
          message,
          open: false,
          // fake the existence of cssMatchingElements to test just MESSAGE_OPEN action
          cssMatchingElements: {},
          dispatch: store.dispatch,
          serviceContainer,
        })
      )
    );

    wrapper.find(".collapse-button[aria-expanded='false']").simulate("click");

    const call = store.dispatch.getCall(0);
    expect(call.args[0]).toEqual({
      id: message.id,
      type: MESSAGE_OPEN,
    });
  });
});
