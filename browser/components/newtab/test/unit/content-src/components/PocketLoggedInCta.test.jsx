import {combineReducers, createStore} from "redux";
import {INITIAL_STATE, reducers} from "common/Reducers.jsm";
import {mountWithIntl, shallowWithIntl} from "test/unit/utils";
import {PocketLoggedInCta, _PocketLoggedInCta as PocketLoggedInCtaRaw} from "content-src/components/PocketLoggedInCta/PocketLoggedInCta";
import {FormattedMessage} from "react-intl";
import {Provider} from "react-redux";
import React from "react";

function mountSectionWithProps(props) {
  const store = createStore(combineReducers(reducers), INITIAL_STATE);
  return mountWithIntl(<Provider store={store}><PocketLoggedInCta {...props} /></Provider>);
}

describe("<PocketLoggedInCta>", () => {
  it("should render a PocketLoggedInCta element", () => {
    const wrapper = mountSectionWithProps({});
    assert.ok(wrapper.exists());
  });
  it("should render FormattedMessages when rendered without props", () => {
    const wrapper = mountSectionWithProps({});

    const message = wrapper.find(FormattedMessage);
    assert.lengthOf(message, 2);
  });
  it("should not render FormattedMessages when rendered with props", () => {
    const wrapper = shallowWithIntl(<PocketLoggedInCtaRaw Pocket={{
      pocketCta: {
        ctaButton: "button",
        ctaText: "text"
      }
    }} />);

    const message = wrapper.find(FormattedMessage);
    assert.lengthOf(message, 0);
  });
});
