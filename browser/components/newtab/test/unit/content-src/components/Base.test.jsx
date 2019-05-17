import {_Base as Base, BaseContent} from "content-src/components/Base/Base";
import {ASRouterAdmin} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import {ErrorBoundary} from "content-src/components/ErrorBoundary/ErrorBoundary";
import React from "react";
import {Search} from "content-src/components/Search/Search";
import {shallow} from "enzyme";

describe("<Base>", () => {
  let DEFAULT_PROPS = {store: {getState: () => {}}, App: {initialized: true}, Prefs: {values: {}}, Sections: [], DiscoveryStream: {config: {enabled: false}}, dispatch: () => {}};

  it("should render Base component", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);
    assert.ok(wrapper.exists());
  });

  it("should render the BaseContent component, passing through all props", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);

    assert.deepEqual(wrapper.find(BaseContent).props(), DEFAULT_PROPS);
  });

  it("should render an ErrorBoundary with class base-content-fallback", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} />);

    assert.equal(
      wrapper.find(ErrorBoundary).first().prop("className"), "base-content-fallback");
  });

  it("should render an ASRouterAdmin if the devtools pref is true", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} Prefs={{values: {"asrouter.devtoolsEnabled": true}}} />);
    assert.lengthOf(wrapper.find(ASRouterAdmin), 1);
  });

  it("should not render an ASRouterAdmin if the devtools pref is false", () => {
    const wrapper = shallow(<Base {...DEFAULT_PROPS} Prefs={{values: {"asrouter.devtoolsEnabled": false}}} />);
    assert.lengthOf(wrapper.find(ASRouterAdmin), 0);
  });
});

describe("<BaseContent>", () => {
  let DEFAULT_PROPS = {store: {getState: () => {}}, App: {initialized: true}, Prefs: {values: {}}, Sections: [], DiscoveryStream: {config: {enabled: false}}, dispatch: () => {}};

  it("should render an ErrorBoundary with a Search child", () => {
    const searchEnabledProps =
      Object.assign({}, DEFAULT_PROPS, {Prefs: {values: {showSearch: true}}});

    const wrapper = shallow(<BaseContent {...searchEnabledProps} />);

    assert.isTrue(wrapper.find(Search).parent().is(ErrorBoundary));
  });

  it("should render only search if no Sections are enabled", () => {
    const onlySearchProps =
      Object.assign({}, DEFAULT_PROPS, {Sections: [{id: "highlights", enabled: false}], Prefs: {values: {showSearch: true}}});

    const wrapper = shallow(<BaseContent {...onlySearchProps} />);
    assert.lengthOf(wrapper.find(".only-search"), 1);
  });

  it("should render only search if only highlights is available in DS", () => {
    const onlySearchProps =
      Object.assign({}, DEFAULT_PROPS, {Sections: [{id: "highlights", enabled: true}], DiscoveryStream: {config: {enabled: true}}, Prefs: {values: {showSearch: true}}});

    const wrapper = shallow(<BaseContent {...onlySearchProps} />);
    assert.lengthOf(wrapper.find(".only-search"), 1);
  });
});
