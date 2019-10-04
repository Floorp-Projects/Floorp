import { DSPrivacyModal } from "content-src/components/DiscoveryStreamComponents/DSPrivacyModal/DSPrivacyModal";
import { shallow, mount } from "enzyme";
import React from "react";

describe("Discovery Stream <DSPrivacyModal>", () => {
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
  });

  it("should contain a privacy notice", () => {
    const modal = mount(<DSPrivacyModal />);
    const child = modal.find(".privacy-notice");

    assert.lengthOf(child, 1);
  });

  it("should call dispatch when modal is closed", () => {
    let dispatch = sandbox.stub();
    let wrapper = shallow(<DSPrivacyModal dispatch={dispatch} />);

    wrapper.instance().closeModal();
    assert.calledOnce(dispatch);
  });
});
