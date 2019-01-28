/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");

const { getCurrentRuntimeDetails } = require("../../modules/runtimes-state-helper");

const InspectAction = createFactory(require("./InspectAction"));

const Actions = require("../../actions/index");
const Types = require("../../types/index");

/**
 * This component displays buttons for service worker.
 */
class ServiceWorkerAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      // Provided by redux state
      runtimeDetails: Types.runtimeDetails.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  push() {
    const { dispatch, target } = this.props;
    dispatch(Actions.pushServiceWorker(target.id));
  }

  start() {
    const { dispatch, target } = this.props;
    dispatch(Actions.startServiceWorker(target.details.registrationFront));
  }

  _renderAction() {
    const { dispatch, runtimeDetails, target } = this.props;
    const { isActive, isRunning } = target.details;
    const { isMultiE10s } = runtimeDetails;

    if (!isRunning) {
      const startLabel = this.props.getString("about-debugging-worker-action-start");
      return this._renderButton({
        className: "default-button js-start-button",
        disabled: isMultiE10s,
        label: startLabel,
        onClick: this.start.bind(this),
      });
    }

    if (!isActive) {
      // Only debug button is available if the service worker is not active.
      return InspectAction({ disabled: isMultiE10s, dispatch, target });
    }

    const pushLabel = this.props.getString("about-debugging-worker-action-push");
    return [
      this._renderButton({
        className: "default-button js-push-button",
        disabled: isMultiE10s,
        label: pushLabel,
        onClick: this.push.bind(this),
      }),
      InspectAction({ disabled: isMultiE10s, dispatch, target }),
    ];
  }

  _renderButton({ className, disabled, label, onClick }) {
    return dom.button(
      {
        className,
        disabled,
        onClick: e => onClick(),
      },
      label,
    );
  }

  render() {
    return dom.div(
      {
        className: "toolbar",
      },
      this._renderAction()
    );
  }
}

const mapStateToProps = state => {
  return {
    runtimeDetails: getCurrentRuntimeDetails(state.runtimes),
  };
};

module.exports = FluentReact.withLocalization(
  connect(mapStateToProps)(ServiceWorkerAction));
