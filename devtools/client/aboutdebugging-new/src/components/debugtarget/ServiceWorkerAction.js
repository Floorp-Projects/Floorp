/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");

const InspectAction = createFactory(require("./InspectAction"));

const Actions = require("../../actions/index");

/**
 * This component displays buttons for service worker.
 */
class ServiceWorkerAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      target: PropTypes.object.isRequired,
    };
  }

  push() {
    const { dispatch, target } = this.props;
    dispatch(Actions.pushServiceWorker(target.id));
  }

  start() {
    const { dispatch, target } = this.props;
    dispatch(Actions.startServiceWorker(target.details.registrationActor));
  }

  _renderAction() {
    const { dispatch, target } = this.props;
    const { isActive, isRunning } = target.details;

    if (!isRunning) {
      const startLabel = this.props.getString("about-debugging-worker-action-start");
      return this._renderButton(startLabel, this.start.bind(this));
    }

    if (!isActive) {
      // Only debug button is available if the service worker is not active.
      return InspectAction({ dispatch, target });
    }

    const pushLabel = this.props.getString("about-debugging-worker-action-push");
    return [
      this._renderButton(pushLabel, this.push.bind(this)),
      InspectAction({ dispatch, target }),
    ];
  }

  _renderButton(label, onClick) {
    return dom.button(
      {
        className: "aboutdebugging-button",
        onClick: e => onClick(),
      },
      label,
    );
  }

  render() {
    return dom.div({}, this._renderAction());
  }
}

module.exports = FluentReact.withLocalization(ServiceWorkerAction);
