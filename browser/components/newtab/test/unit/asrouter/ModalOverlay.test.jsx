import {ModalOverlayWrapper} from "content-src/asrouter/components/ModalOverlay/ModalOverlay";
import {mount} from "enzyme";
import React from "react";

describe("ModalOverlayWrapper", () => {
  let fakeDoc;
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    fakeDoc = {
      addEventListener: sandbox.stub(),
      removeEventListener: sandbox.stub(),
      body: {classList: {add: sandbox.stub(), remove: sandbox.stub()}},
    };
  });
  afterEach(() => {
    sandbox.restore();
  });
  it("should add eventListener and a class on mount", async () => {
    mount(<ModalOverlayWrapper document={fakeDoc} />);
    assert.calledOnce(fakeDoc.addEventListener);
    assert.calledWith(fakeDoc.body.classList.add, "modal-open");
  });

  it("should remove eventListener on unmount", async () => {
    const wrapper = mount(<ModalOverlayWrapper document={fakeDoc} />);
    wrapper.unmount();
    assert.calledOnce(fakeDoc.addEventListener);
    assert.calledOnce(fakeDoc.removeEventListener);
    assert.calledWith(fakeDoc.body.classList.remove, "modal-open");
  });

  it("should call props.onClose on an Escape key", async () => {
    const onClose = sandbox.stub();
    mount(<ModalOverlayWrapper document={fakeDoc} onClose={onClose} />);

    // Simulate onkeydown being called
    const [, callback] = fakeDoc.addEventListener.firstCall.args;
    callback({key: "Escape"});

    assert.calledOnce(onClose);
  });

  it("should not call props.onClose on other keys than Escape", async () => {
    const onClose = sandbox.stub();
    mount(<ModalOverlayWrapper document={fakeDoc} onClose={onClose} />);

    // Simulate onkeydown being called
    const [, callback] = fakeDoc.addEventListener.firstCall.args;
    callback({key: "Ctrl"});

    assert.notCalled(onClose);
  });
});
