import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {_ConfirmDialog as ConfirmDialog} from "content-src/components/ConfirmDialog/ConfirmDialog";
import {FormattedMessage} from "react-intl";
import React from "react";
import {shallowWithIntl} from "test/unit/utils";

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
        cancel_button_string_id: "manual_migration_cancel_button",
        confirm_button_string_id: "manual_migration_import_button",
        eventSource: "HIGHLIGHTS"
      }
    };
    wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);
  });
  it("should render an overlay", () => {
    assert.ok(wrapper.find(".modal-overlay").exists());
  });
  it("should render a modal", () => {
    assert.ok(wrapper.find(".confirmation-dialog").exists());
  });
  it("should not render if visible is false", () => {
    ConfirmDialogProps.visible = false;
    wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);

    assert.lengthOf(wrapper.find(".confirmation-dialog"), 0);
  });
  it("should display an icon if we provide one in props", () => {
    const iconName = "modal-icon";
    // If there is no icon in the props, we shouldn't display an icon
    assert.lengthOf(wrapper.find(`.icon-${iconName}`), 0);

    ConfirmDialogProps.data.icon = iconName;
    wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);

    // But if we do provide an icon - we should show it
    assert.lengthOf(wrapper.find(`.icon-${iconName}`), 1);
  });
  describe("intl message check", () => {
    it("should render the message body sent via props", () => {
      Object.assign(ConfirmDialogProps.data, {body_string_id: ["foo", "bar"]});
      wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);

      let msgs = wrapper.find(".modal-message").find(FormattedMessage);
      assert.equal(msgs.length, ConfirmDialogProps.data.body_string_id.length);

      msgs.forEach((fm, i) => assert.equal(fm.props().id, ConfirmDialogProps.data.body_string_id[i]));
    });
    it("should render the correct primary button text", () => {
      Object.assign(ConfirmDialogProps.data, {confirm_button_string_id: "primary_foo"});
      wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);

      let doneLabel = wrapper.find(".actions").childAt(1).find(FormattedMessage);
      assert.ok(doneLabel.exists());
      assert.equal(doneLabel.props().id, ConfirmDialogProps.data.confirm_button_string_id);
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
      assert.calledWith(dispatch, {type: at.DIALOG_CANCEL});
    });
    it("should emit UserEvent DIALOG_CANCEL when you click the overlay", () => {
      let overlay = wrapper.find(".modal-overlay");

      assert.ok(overlay);
      overlay.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.isUserEventAction(dispatch.secondCall.args[0]);
      assert.calledWith(dispatch, ac.UserEvent({event: at.DIALOG_CANCEL, source: "HIGHLIGHTS"}));
    });
    it("should emit AlsoToMain DIALOG_CANCEL on cancel", () => {
      let cancelButton = wrapper.find(".actions").childAt(0);

      assert.ok(cancelButton);
      cancelButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.propertyVal(dispatch.firstCall.args[0], "type", at.DIALOG_CANCEL);
      assert.calledWith(dispatch, {type: at.DIALOG_CANCEL});
    });
    it("should emit UserEvent DIALOG_CANCEL on cancel", () => {
      let cancelButton = wrapper.find(".actions").childAt(0);

      assert.ok(cancelButton);
      cancelButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.isUserEventAction(dispatch.secondCall.args[0]);
      assert.calledWith(dispatch, ac.UserEvent({event: at.DIALOG_CANCEL, source: "HIGHLIGHTS"}));
    });
    it("should emit UserEvent on primary button", () => {
      Object.assign(ConfirmDialogProps.data, {
        body_string_id: ["foo", "bar"],
        onConfirm: [
          ac.AlsoToMain({type: at.DELETE_URL, data: "foo.bar"}),
          ac.UserEvent({event: "DELETE"})
        ]
      });
      wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);
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
          ac.AlsoToMain({type: at.DELETE_URL, data: "foo.bar"}),
          ac.UserEvent({event: "DELETE"})
        ]
      });
      wrapper = shallowWithIntl(<ConfirmDialog dispatch={dispatch} {...ConfirmDialogProps} />);
      let doneButton = wrapper.find(".actions").childAt(1);

      assert.ok(doneButton);
      doneButton.simulate("click");

      // Two events are emitted: UserEvent+AlsoToMain.
      assert.calledTwice(dispatch);
      assert.calledWith(dispatch, ConfirmDialogProps.data.onConfirm[0]);
    });
  });
});
