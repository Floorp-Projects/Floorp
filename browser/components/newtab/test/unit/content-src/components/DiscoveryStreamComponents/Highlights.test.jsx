import {combineReducers, createStore} from "redux";
import {INITIAL_STATE, reducers} from "common/Reducers.jsm";
import {Highlights} from "content-src/components/DiscoveryStreamComponents/Highlights/Highlights";
import {mountWithIntl} from "test/unit/utils";
import {Provider} from "react-redux";
import React from "react";

describe("Discovery Stream <Highlights>", () => {
  let wrapper;

  afterEach(() => {
    wrapper.unmount();
  });

  it("should render nothing with no highlights data", () => {
    const store = createStore(combineReducers(reducers), {...INITIAL_STATE});

    wrapper = mountWithIntl(<Provider store={store}><Highlights /></Provider>);

    assert.ok(wrapper.isEmptyRender());
  });

  it("should render highlights", () => {
    const store = createStore(combineReducers(reducers), {
      ...INITIAL_STATE,
      Sections: [{id: "highlights", enabled: true}],
    });

    wrapper = mountWithIntl(<Provider store={store}><Highlights /></Provider>);

    assert.lengthOf(wrapper.find(".ds-highlights"), 1);
  });
});
