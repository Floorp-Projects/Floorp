import {actionCreators as ac, actionTypes} from "common/Actions.jsm";
import {connect} from "react-redux";
import {FormattedMessage} from "react-intl";
import React from "react";

/**
 * ConfirmDialog component.
 * One primary action button, one cancel button.
 *
 * Content displayed is controlled by `data` prop the component receives.
 * Example:
 * data: {
 *   // Any sort of data needed to be passed around by actions.
 *   payload: site.url,
 *   // Primary button AlsoToMain action.
 *   action: "DELETE_HISTORY_URL",
 *   // Primary button USerEvent action.
 *   userEvent: "DELETE",
 *   // Array of locale ids to display.
 *   message_body: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
 *   // Text for primary button.
 *   confirm_button_string_id: "menu_action_delete"
 * },
 */
export class _ConfirmDialog extends React.PureComponent {
  constructor(props) {
    super(props);
    this._handleCancelBtn = this._handleCancelBtn.bind(this);
    this._handleConfirmBtn = this._handleConfirmBtn.bind(this);
  }

  _handleCancelBtn() {
    this.props.dispatch({type: actionTypes.DIALOG_CANCEL});
    this.props.dispatch(ac.UserEvent({event: actionTypes.DIALOG_CANCEL, source: this.props.data.eventSource}));
  }

  _handleConfirmBtn() {
    this.props.data.onConfirm.forEach(this.props.dispatch);
  }

  _renderModalMessage() {
    const message_body = this.props.data.body_string_id;

    if (!message_body) {
      return null;
    }

    return (<span>
      {message_body.map(msg => <p key={msg}><FormattedMessage id={msg} /></p>)}
    </span>);
  }

  render() {
    if (!this.props.visible) {
      return null;
    }

    return (<div className="confirmation-dialog">
      <div className="modal-overlay" onClick={this._handleCancelBtn} />
      <div className="modal">
        <section className="modal-message">
          {this.props.data.icon && <span className={`icon icon-spacer icon-${this.props.data.icon}`} />}
          {this._renderModalMessage()}
        </section>
        <section className="actions">
          <button onClick={this._handleCancelBtn}>
            <FormattedMessage id={this.props.data.cancel_button_string_id} />
          </button>
          <button className="done" onClick={this._handleConfirmBtn}>
            <FormattedMessage id={this.props.data.confirm_button_string_id} />
          </button>
        </section>
      </div>
    </div>);
  }
}

export const ConfirmDialog = connect(state => state.Dialog)(_ConfirmDialog);
