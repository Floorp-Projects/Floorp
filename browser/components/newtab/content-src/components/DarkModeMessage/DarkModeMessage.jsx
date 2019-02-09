import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {connect} from "react-redux";
import React from "react";

export class _DarkModeMessage extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleSwitch = this.handleSwitch.bind(this);
    this.handleCancel = this.handleCancel.bind(this);
  }

  handleSwitch() {
    // Switch to default new tab version
    this.props.dispatch(ac.AlsoToMain({type: at.DISCOVERY_STREAM_OPT_OUT}));
  }

  handleCancel() {
    // Capture user consent and not show dark mode message in future
    this.props.dispatch(ac.SetPref("darkModeMessage", false));
  }

  render() {
    return (<div className="ds-message-container">
        <p>
          <span className="icon icon-info" />
          <span>This version of New Tab doesn't support dark mode yet.</span>
        </p>
        <div className="ds-message-actions actions">
          <button onClick={this.handleCancel}>
            <span>Got it</span>
          </button>
          <button className="dismiss" onClick={this.handleSwitch}>
            <span>Use older version</span>
          </button>
        </div>
    </div>);
  }
}

export const DarkModeMessage = connect()(_DarkModeMessage);
