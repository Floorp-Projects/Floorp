import { DSDismiss } from "content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss";
import React from "react";
import { shallow } from "enzyme";

describe("<DSTextPromo>", () => {
  const fakeSpoc = {
    url: "https://foo.com",
    guid: "1234",
  };
  let wrapper;
  let sandbox;
  let dispatchStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    wrapper = shallow(
      <DSDismiss
        data={fakeSpoc}
        dispatch={dispatchStub}
        shouldSendImpressionStats={true}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-dismiss").exists());
  });

  it("should render proper hover state", () => {
    wrapper.instance().onHover();
    assert.ok(wrapper.find(".hovering").exists());
    wrapper.instance().offHover();
    assert.ok(!wrapper.find(".hovering").exists());
  });

  it("should dispatch a BlockUrl event on click", () => {
    wrapper.instance().onDismissClick();

    assert.calledThrice(dispatchStub);
    assert.deepEqual(dispatchStub.firstCall.args[0].data, {
      url: "https://foo.com",
      pocket_id: undefined,
    });
    assert.deepEqual(dispatchStub.secondCall.args[0].data, {
      event: "BLOCK",
      source: "DISCOVERY_STREAM",
      action_position: 0,
      url: "https://foo.com",
      guid: "1234",
    });
    assert.deepEqual(dispatchStub.thirdCall.args[0].data, {
      source: "DISCOVERY_STREAM",
      block: 0,
      tiles: [
        {
          id: "1234",
          pos: 0,
        },
      ],
    });
  });
});
