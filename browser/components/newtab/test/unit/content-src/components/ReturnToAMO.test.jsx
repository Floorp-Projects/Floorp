import {mountWithIntl} from "test/unit/utils";
import React from "react";
import {ReturnToAMO} from "content-src/asrouter/templates/ReturnToAMO/ReturnToAMO";

describe("<ReturnToAMO>", () => {
  let dispatch;
  let onReady;
  let sandbox;
  let wrapper;
  let sendUserActionTelemetryStub;
  let content;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    onReady = sandbox.stub();
    sendUserActionTelemetryStub = sandbox.stub();
    content = {
      primary_button: {},
      secondary_button: {},
    };
  });

  afterEach(() => {
    sandbox.restore();
  });

  describe("not mounted", () => {
    it("should send an IMPRESSION on mount", () => {
      assert.notCalled(sendUserActionTelemetryStub);

      wrapper = mountWithIntl(<ReturnToAMO onReady={onReady}
        dispatch={dispatch}
        content={content}
        onBlock={sandbox.stub()}
        onAction={sandbox.stub()}
        UISurface="NEWTAB_OVERLAY"
        sendUserActionTelemetry={sendUserActionTelemetryStub} />);

      assert.calledOnce(sendUserActionTelemetryStub);
      assert.calledWithExactly(sendUserActionTelemetryStub, {
        event: "IMPRESSION",
        id: wrapper.instance().props.UISurface,
      });
    });
  });

  describe("mounted", () => {
    beforeEach(() => {
      wrapper = mountWithIntl(<ReturnToAMO onReady={onReady}
        dispatch={dispatch}
        content={content}
        onBlock={sandbox.stub()}
        onAction={sandbox.stub()}
        UISurface="NEWTAB_OVERLAY"
        sendUserActionTelemetry={sendUserActionTelemetryStub} />);

      // Clear the IMPRESSION ping
      sendUserActionTelemetryStub.reset();
    });

    it("should call onReady on componentDidMount", () => {
      assert.calledOnce(onReady);
    });

    it("should send telemetry on block", () => {
      wrapper.instance().onBlockButton();

      assert.calledOnce(sendUserActionTelemetryStub);
      assert.calledWithExactly(sendUserActionTelemetryStub, {
        event: "BLOCK",
        id: wrapper.instance().props.UISurface,
      });
    });

    it("should send telemetry on install", () => {
      wrapper.instance().onClickAddExtension();

      assert.calledWithExactly(sendUserActionTelemetryStub, {
        event: "INSTALL",
        id: wrapper.instance().props.UISurface,
      });
    });
  });
});
