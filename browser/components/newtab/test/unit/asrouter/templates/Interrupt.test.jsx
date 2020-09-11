import { FullPageInterrupt } from "content-src/asrouter/templates/FullPageInterrupt/FullPageInterrupt";
import { Interrupt } from "content-src/asrouter/templates/FirstRun/Interrupt";
import { ReturnToAMO } from "content-src/asrouter/templates/ReturnToAMO/ReturnToAMO";
import { Trailhead } from "content-src/asrouter/templates//Trailhead/Trailhead";
import { shallow } from "enzyme";
import React from "react";

describe("<Interrupt>", () => {
  let wrapper;
  it("should render Return TO AMO when the message has a template of return_to_amo_overlay", () => {
    wrapper = shallow(
      <Interrupt
        message={{ id: "FOO", content: {}, template: "return_to_amo_overlay" }}
      />
    );
    assert.lengthOf(wrapper.find(ReturnToAMO), 1);
  });
  it("should render Trailhead when the message has a template of trailhead", () => {
    wrapper = shallow(
      <Interrupt message={{ id: "FOO", content: {}, template: "trailhead" }} />
    );
    assert.lengthOf(wrapper.find(Trailhead), 1);
  });
  it("should render Full Page interrupt when the message has a template of full_page_interrupt", () => {
    wrapper = shallow(
      <Interrupt
        message={{ id: "FOO", content: {}, template: "full_page_interrupt" }}
      />
    );
    assert.lengthOf(wrapper.find(FullPageInterrupt), 1);
  });
  it("should throw an error if another type of message is dispatched", () => {
    assert.throws(() => {
      wrapper = shallow(
        <Interrupt message={{ id: "FOO", template: "something" }} />
      );
    });
  });
});
