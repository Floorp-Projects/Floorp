import { DSSignup } from "content-src/components/DiscoveryStreamComponents/DSSignup/DSSignup";
import React from "react";
import { shallow } from "enzyme";

describe("<DSSignup>", () => {
  let wrapper;
  let sandbox;
  let dispatchStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    wrapper = shallow(
      <DSSignup
        data={{
          spocs: [
            {
              shim: { impression: "1234" },
              id: "1234",
            },
          ],
        }}
        type="SIGNUP"
        dispatch={dispatchStub}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-signup").exists());
  });

  it("should dispatch a click event on click", () => {
    wrapper.instance().onLinkClick();

    assert.calledTwice(dispatchStub);
    assert.deepEqual(dispatchStub.firstCall.args[0].data, {
      event: "CLICK",
      source: "SIGNUP",
      action_position: 0,
    });
    assert.deepEqual(dispatchStub.secondCall.args[0].data, {
      source: "SIGNUP",
      click: 0,
      tiles: [{ id: "1234", pos: 0 }],
    });
  });

  it("Should remove active on Menu Update", () => {
    wrapper.setState = sandbox.stub();
    wrapper.instance().onMenuButtonUpdate(false);
    assert.calledWith(wrapper.setState, { active: false, lastItem: false });
  });

  it("Should add active on Menu Show", async () => {
    wrapper.setState = sandbox.stub();
    wrapper.instance().nextAnimationFrame = () => {};
    await wrapper.instance().onMenuShow();
    assert.calledWith(wrapper.setState, { active: true, lastItem: false });
  });

  it("Should add last-item to support resized window", async () => {
    const fakeWindow = { scrollMaxX: "20" };
    wrapper = shallow(<DSSignup windowObj={fakeWindow} />);
    wrapper.setState = sandbox.stub();
    wrapper.instance().nextAnimationFrame = () => {};
    await wrapper.instance().onMenuShow();
    assert.calledWith(wrapper.setState, { active: true, lastItem: true });
  });

  it("Should add last-item and active classes", () => {
    wrapper.setState({
      active: true,
      lastItem: true,
    });
    assert.ok(wrapper.find(".last-item").exists());
    assert.ok(wrapper.find(".active").exists());
  });

  it("Should call rAF from nextAnimationFrame", () => {
    const fakeWindow = { requestAnimationFrame: sinon.stub() };
    wrapper = shallow(<DSSignup windowObj={fakeWindow} />);

    wrapper.instance().nextAnimationFrame();
    assert.calledOnce(fakeWindow.requestAnimationFrame);
  });
});
