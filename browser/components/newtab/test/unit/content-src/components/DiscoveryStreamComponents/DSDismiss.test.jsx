import { DSDismiss } from "content-src/components/DiscoveryStreamComponents/DSDismiss/DSDismiss";
import React from "react";
import { shallow } from "enzyme";

describe("<DSDismiss>", () => {
  const fakeSpoc = {
    url: "https://foo.com",
    guid: "1234",
  };
  let wrapper;
  let sandbox;
  let onDismissClickStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    onDismissClickStub = sandbox.stub();
    wrapper = shallow(
      <DSDismiss
        data={fakeSpoc}
        onDismissClick={onDismissClickStub}
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

  it("should dispatch call onDismissClick", () => {
    wrapper.instance().onDismissClick();
    assert.calledOnce(onDismissClickStub);
  });

  it("should add extra classes", () => {
    wrapper = shallow(<DSDismiss extraClasses="extra-class" />);
    assert.ok(wrapper.find(".extra-class").exists());
  });
});
