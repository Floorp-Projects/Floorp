import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { mount } from "enzyme";
import React from "react";
import { StartupOverlay } from "content-src/asrouter/templates/StartupOverlay/StartupOverlay";

describe("<StartupOverlay>", () => {
  let wrapper;
  let fakeDocument;
  let dummyNode;
  let dispatch;
  let onBlock;
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    onBlock = sandbox.stub();

    dummyNode = document.createElement("body");
    sandbox.stub(dummyNode, "querySelector").returns(dummyNode);
    fakeDocument = {
      get activeElement() {
        return dummyNode;
      },
      get body() {
        return dummyNode;
      },
      getElementById() {
        return dummyNode;
      },
    };

    wrapper = mount(
      <StartupOverlay
        onBlock={onBlock}
        dispatch={dispatch}
        document={fakeDocument}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should add show class after mount and timeout", async () => {
    const clock = sandbox.useFakeTimers();
    // We need to mount here to trigger ComponentDidMount after the FakeTimers are added.
    wrapper = mount(
      <StartupOverlay
        onBlock={onBlock}
        dispatch={dispatch}
        document={fakeDocument}
      />
    );
    assert.isFalse(
      wrapper.find(".overlay-wrapper").hasClass("show"),
      ".overlay-wrapper does not have .show class"
    );

    clock.tick(10);
    wrapper.update();

    assert.isTrue(
      wrapper.find(".overlay-wrapper").hasClass("show"),
      ".overlay-wrapper has .show class"
    );
  });

  it("should emit UserEvent SKIPPED_SIGNIN when you click the skip button", () => {
    let skipButton = wrapper.find(".skip-button");
    assert.ok(skipButton.exists());
    skipButton.simulate("click");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: at.SKIPPED_SIGNIN,
        value: { has_flow_params: false },
      })
    );
  });

  it("should emit UserEvent SUBMIT_EMAIL when you submit the form", () => {
    let form = wrapper.find("form");
    assert.ok(form.exists());
    form.simulate("submit");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: at.SUBMIT_EMAIL,
        value: { has_flow_params: false },
      })
    );
  });
});
