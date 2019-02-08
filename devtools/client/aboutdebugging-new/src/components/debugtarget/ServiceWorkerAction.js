/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { getCurrentRuntimeDetails } = require("../../modules/runtimes-state-helper");

const InspectAction = createFactory(require("./InspectAction"));

const Actions = require("../../actions/index");
const Types = require("../../types/index");
const { SERVICE_WORKER_STATUSES } = require("../../constants");

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

  unregister() {
    const { dispatch, target } = this.props;
    dispatch(Actions.unregisterServiceWorker(target.details.registrationFront));
  }

  _renderButton({ className, disabled, key, labelId, onClick }) {
    return Localized(
      {
        id: labelId,
        key,
      },
      dom.button(
        {
          className,
          disabled,
          onClick: e => onClick(),
        },
        labelId,
      )
    );
  }

  _renderInspectAction() {
    return InspectAction({
      disabled: this.props.runtimeDetails.isMultiE10s,
      dispatch: this.props.dispatch,
      key: "service-worker-inspect-action",
      target: this.props.target,
    });
  }

  _renderPushButton() {
    return this._renderButton({
      className: "default-button js-push-button",
      disabled: this.props.runtimeDetails.isMultiE10s,
      key: "service-worker-push-button",
      labelId: "about-debugging-worker-action-push",
      onClick: this.push.bind(this),
    });
  }

  _renderStartButton() {
    return this._renderButton({
      className: "default-button js-start-button",
      disabled: this.props.runtimeDetails.isMultiE10s,
      key: "service-worker-start-button",
      labelId: "about-debugging-worker-action-start",
      onClick: this.start.bind(this),
    });
  }

  _renderUnregisterButton() {
    return this._renderButton({
      className: "default-button js-unregister-button",
      key: "service-worker-unregister-button",
      labelId: "about-debugging-worker-action-unregister",
      onClick: this.unregister.bind(this),
    });
  }

  _renderAction() {
    const { status } = this.props.target.details;

    switch (status) {
      case SERVICE_WORKER_STATUSES.RUNNING:
        return [
          this._renderUnregisterButton(),
          this._renderPushButton(),
          this._renderInspectAction(),
        ];
      case SERVICE_WORKER_STATUSES.REGISTERING:
        // Only inspect is available if the service worker is not active.
        return [
          this._renderInspectAction(),
        ];
      case SERVICE_WORKER_STATUSES.STOPPED:
        return [
          this._renderUnregisterButton(),
          this._renderStartButton(),
        ];
      default:
        console.error("Unexpected service worker status: " + status);
        return [];
    }
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
