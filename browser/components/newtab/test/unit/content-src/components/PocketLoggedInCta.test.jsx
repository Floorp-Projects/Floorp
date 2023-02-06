import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";
import { mount, shallow } from "enzyme";
import {
  PocketLoggedInCta,
  _PocketLoggedInCta as PocketLoggedInCtaRaw,
} from "content-src/components/PocketLoggedInCta/PocketLoggedInCta";
import { Provider } from "react-redux";
import React from "react";

function mountSectionWithProps(props) {
  const store = createStore(combineReducers(reducers), INITIAL_STATE);
  return mount(
    <Provider store={store}>
      <PocketLoggedInCta {...props} />
    </Provider>
  );
}

describe("<PocketLoggedInCta>", () => {
  it("should render a PocketLoggedInCta element", () => {
    const wrapper = mountSectionWithProps({});
    assert.ok(wrapper.exists());
  });
  it("should render Fluent spans when rendered without props", () => {
    const wrapper = mountSectionWithProps({});

    const message = wrapper.find("span[data-l10n-id]");
    assert.lengthOf(message, 2);
  });
  it("should not render Fluent spans when rendered with props", () => {
    const wrapper = shallow(
      <PocketLoggedInCtaRaw
        Pocket={{
          pocketCta: {
            ctaButton: "button",
            ctaText: "text",
          },
        }}
      />
    );

    const message = wrapper.find("span[data-l10n-id]");
    assert.lengthOf(message, 0);
  });
});
