import { actionCreators as ac } from "common/Actions.jsm";
import { ContentSection } from "content-src/components/CustomizeMenu/ContentSection/ContentSection";
import { shallow } from "enzyme";
import React from "react";

const DEFAULT_PROPS = {
  enabledSections: {},
  dispatch: sinon.stub(),
  setPref: sinon.stub(),
};

describe("ContentSection", () => {
  let wrapper;
  beforeEach(() => {
    wrapper = shallow(<ContentSection {...DEFAULT_PROPS} />);
  });

  it("should render the component", () => {
    assert.ok(wrapper.exists());
  });

  it("should dispatch UserEvent for INPUT", () => {
    wrapper.instance().onPreferenceSelect({
      target: {
        nodeName: "INPUT",
        checked: true,
        getAttribute: attribute => attribute,
      },
    });

    assert.calledWith(
      DEFAULT_PROPS.dispatch,
      ac.UserEvent({
        event: "PREF_CHANGED",
        source: "eventSource",
        value: { status: true, menu_source: "CUSTOMIZE_MENU" },
      })
    );
  });
});
