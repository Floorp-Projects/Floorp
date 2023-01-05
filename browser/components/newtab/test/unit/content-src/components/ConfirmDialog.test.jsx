import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { _ConfirmDialog as ConfirmDialog } from "content-src/components/ConfirmDialog/ConfirmDialog";
import React from "react";
import { shallow } from "enzyme";

describe("<ConfirmDialog>", () => {
  let wrapper;
  let dispatch;
  let ConfirmDialogProps;
  beforeEach(() => {
    dispatch = sinon.stub();
    ConfirmDialogProps = {
      visible: true,
      data: {
        onConfirm: [],
        cancel_button_string_id: "newtab-topsites-delete-history-button",
        confirm_button_string_id: "newtab-topsites-cancel-button",
        eventSource: "HIGHLIGHTS",
      },
    };
    wrapper = shallow(
      <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
    );
  });
  it("should render an overlay", () => {
    assert.ok(wrapper.find(".modal-overlay").exists());
  });
  it("should render a modal", () => {
    assert.ok(wrapper.find(".confirmation-dialog").exists());
  });
  it("should not render if visible is false", () => {
    ConfirmDialogProps.visible = false;
    wrapper = shallow(
      <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
    );

    assert.lengthOf(wrapper.find(".confirmation-dialog"), 0);
  });
  it("should display an icon if we provide one in props", () => {
    const iconName = "modal-icon";
    // If there is no icon in the props, we shouldn't display an icon
    assert.lengthOf(wrapper.find(`.icon-${iconName}`), 0);

    ConfirmDialogProps.data.icon = iconName;
    wrapper = shallow(
      <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
    );

    // But if we do provide an icon - we should show it
    assert.lengthOf(wrapper.find(`.icon-${iconName}`), 1);
  });
  describe("fluent message check", () => {
    it("should render the message body sent via props", () => {
      Object.assign(ConfirmDialogProps.data, {
        body_string_id: ["foo", "bar"],
      });
      wrapper = shallow(
        <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
      );
      let msgs = wrapper.find(".modal-message").find("p");
      assert.equal(msgs.length, ConfirmDialogProps.data.body_string_id.length);
      msgs.forEach((fm, i) =>
        assert.equal(
          fm.prop("data-l10n-id"),
          ConfirmDialogProps.data.body_string_id[i]
        )
      );
    });
    it("should render the correct primary button text", () => {
      Object.assign(ConfirmDialogProps.data, {
        confirm_button_string_id: "primary_foo",
      });
      wrapper = shallow(
        <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
      );

      let doneLabel = wrapper.find(".actions").childAt(1);
      assert.ok(doneLabel.exists());
      assert.equal(
        doneLabel.prop("data-l10n-id"),
        ConfirmDialogProps.data.confirm_button_string_id
      );
    });
  });
  describe("click events", () => {
    it("should emit AlsoToMain DIALOG_CANCEL when you click the overlay", () => {
      let overlay = wrapper.find(".modal-overlay");

      assert.ok(overlay.exists());
      overlay.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.propertyVal(dispatch.firstCall.args[0], "type", at.DIALOG_CANCEL);
      assert.calledWith(dispatch, { type: at.DIALOG_CANCEL });
    });
    it("should emit UserEvent DIALOG_CANCEL when you click the overlay", () => {
      let overlay = wrapper.find(".modal-overlay");

      assert.ok(overlay);
      overlay.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.isUserEventAction(dispatch.secondCall.args[0]);
      assert.calledWith(
        dispatch,
        ac.UserEvent({ event: at.DIALOG_CANCEL, source: "HIGHLIGHTS" })
      );
    });
    it("should emit AlsoToMain DIALOG_CANCEL on cancel", () => {
      let cancelButton = wrapper.find(".actions").childAt(0);

      assert.ok(cancelButton);
      cancelButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.propertyVal(dispatch.firstCall.args[0], "type", at.DIALOG_CANCEL);
      assert.calledWith(dispatch, { type: at.DIALOG_CANCEL });
    });
    it("should emit UserEvent DIALOG_CANCEL on cancel", () => {
      let cancelButton = wrapper.find(".actions").childAt(0);

      assert.ok(cancelButton);
      cancelButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.isUserEventAction(dispatch.secondCall.args[0]);
      assert.calledWith(
        dispatch,
        ac.UserEvent({ event: at.DIALOG_CANCEL, source: "HIGHLIGHTS" })
      );
    });
    it("should emit UserEvent on primary button", () => {
      Object.assign(ConfirmDialogProps.data, {
        body_string_id: ["foo", "bar"],
        onConfirm: [
          ac.AlsoToMain({ type: at.DELETE_URL, data: "foo.bar" }),
          ac.UserEvent({ event: "DELETE" }),
        ],
      });
      wrapper = shallow(
        <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
      );
      let doneButton = wrapper.find(".actions").childAt(1);

      assert.ok(doneButton);
      doneButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.isUserEventAction(dispatch.secondCall.args[0]);

      assert.calledTwice(dispatch);
      assert.calledWith(dispatch, ConfirmDialogProps.data.onConfirm[1]);
    });
    it("should emit AlsoToMain on primary button", () => {
      Object.assign(ConfirmDialogProps.data, {
        body_string_id: ["foo", "bar"],
        onConfirm: [
          ac.AlsoToMain({ type: at.DELETE_URL, data: "foo.bar" }),
          ac.UserEvent({ event: "DELETE" }),
        ],
      });
      wrapper = shallow(
        <ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />
      );
      let doneButton = wrapper.find(".actions").childAt(1);

      assert.ok(doneButton);
      doneButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.calledWith(dispatch, ConfirmDialogProps.data.onConfirm[0]);
    });
  });
});
