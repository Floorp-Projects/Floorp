import { DSTextPromo } from "content-src/components/DiscoveryStreamComponents/DSTextPromo/DSTextPromo";
import React from "react";
import { shallow } from "enzyme";

describe("<DSTextPromo>", () => {
  let wrapper;
  let sandbox;
  let dispatchStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    wrapper = shallow(
      <DSTextPromo
        shim={{ impression: "1234" }}
        type="TEXTPROMO"
        pos={0}
        dispatch={dispatchStub}
        id="1234"
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-text-promo").exists());
  });

  it("should render a header", () => {
    wrapper.setProps({ header: "foo" });
    assert.ok(wrapper.find(".text").exists());
  });

  it("should render a subtitle", () => {
    wrapper.setProps({ subtitle: "foo" });
    assert.ok(wrapper.find(".subtitle").exists());
  });

  it("should dispatch a click event on click", () => {
    wrapper.instance().onLinkClick();

    assert.calledTwice(dispatchStub);
    assert.deepEqual(dispatchStub.firstCall.args[0].data, {
      event: "CLICK",
      source: "TEXTPROMO",
      action_position: 0,
    });
    assert.deepEqual(dispatchStub.secondCall.args[0].data, {
      source: "TEXTPROMO",
      click: 0,
      tiles: [{ id: "1234", pos: 0 }],
    });
  });
});
