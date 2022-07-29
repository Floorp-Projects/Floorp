import { DSPrivacyModal } from "content-src/components/DiscoveryStreamComponents/DSPrivacyModal/DSPrivacyModal";
import { shallow, mount } from "enzyme";
import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";

describe("Discovery Stream <DSPrivacyModal>", () => {
  let sandbox;
  let dispatch;
  let wrapper;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    wrapper = shallow(<DSPrivacyModal dispatch={dispatch} />);
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should contain a privacy notice", () => {
    const modal = mount(<DSPrivacyModal />);
    const child = modal.find(".privacy-notice");

    assert.lengthOf(child, 1);
  });

  it("should call dispatch when modal is closed", () => {
    wrapper.instance().closeModal();
    assert.calledOnce(dispatch);
  });

  it("should call dispatch with the correct events for onLearnLinkClick", () => {
    wrapper.instance().onLearnLinkClick();

    assert.calledOnce(dispatch);
    assert.calledWith(
      dispatch,
      ac.DiscoveryStreamUserEvent({
        event: "CLICK_PRIVACY_INFO",
        source: "DS_PRIVACY_MODAL",
      })
    );
  });

  it("should call dispatch with the correct events for onManageLinkClick", () => {
    wrapper.instance().onManageLinkClick();

    assert.calledOnce(dispatch);
  });
});
