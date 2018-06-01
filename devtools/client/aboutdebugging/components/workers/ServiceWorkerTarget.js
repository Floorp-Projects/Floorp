/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

class ServiceWorkerTarget extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      debugDisabled: PropTypes.bool,
      target: PropTypes.shape({
        active: PropTypes.bool,
        fetch: PropTypes.bool.isRequired,
        icon: PropTypes.string,
        name: PropTypes.string.isRequired,
        url: PropTypes.string,
        scope: PropTypes.string.isRequired,
        // registrationActor can be missing in e10s.
        registrationActor: PropTypes.string,
        workerTargetActor: PropTypes.string
      }).isRequired
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      pushSubscription: null
    };

    this.debug = this.debug.bind(this);
    this.push = this.push.bind(this);
    this.start = this.start.bind(this);
    this.unregister = this.unregister.bind(this);
    this.onPushSubscriptionModified = this.onPushSubscriptionModified.bind(this);
    this.updatePushSubscription = this.updatePushSubscription.bind(this);
    this.isRunning = this.isRunning.bind(this);
    this.isActive = this.isActive.bind(this);
    this.getServiceWorkerStatus = this.getServiceWorkerStatus.bind(this);
    this.renderButtons = this.renderButtons.bind(this);
    this.renderUnregisterLink = this.renderUnregisterLink.bind(this);
  }

  componentDidMount() {
    const { client } = this.props;
    client.addListener("push-subscription-modified", this.onPushSubscriptionModified);
    this.updatePushSubscription();
  }

  componentDidUpdate(oldProps, oldState) {
    const wasActive = oldProps.target.active;
    if (!wasActive && this.isActive()) {
      // While the service worker isn't active, any calls to `updatePushSubscription`
      // won't succeed. If we just became active, make sure we didn't miss a push
      // subscription change by updating it now.
      this.updatePushSubscription();
    }
  }

  componentWillUnmount() {
    const { client } = this.props;
    client.removeListener("push-subscription-modified", this.onPushSubscriptionModified);
  }

  debug() {
    if (!this.isRunning()) {
      // If the worker is not running, we can't debug it.
      return;
    }

    const { client, target } = this.props;
    gDevToolsBrowser.openWorkerToolbox(client, target.workerTargetActor);
  }

  push() {
    if (!this.isActive() || !this.isRunning()) {
      // If the worker is not running, we can't push to it.
      // If the worker is not active, the registration might be unavailable and the
      // push will not succeed.
      return;
    }

    const { client, target } = this.props;
    client.request({
      to: target.workerTargetActor,
      type: "push"
    });
  }

  start() {
    if (!this.isActive() || this.isRunning()) {
      // If the worker is not active or if it is already running, we can't start it.
      return;
    }

    const { client, target } = this.props;
    client.request({
      to: target.registrationActor,
      type: "start"
    });
  }

  unregister() {
    const { client, target } = this.props;
    client.request({
      to: target.registrationActor,
      type: "unregister"
    });
  }

  onPushSubscriptionModified(type, data) {
    const { target } = this.props;
    if (data.from === target.registrationActor) {
      this.updatePushSubscription();
    }
  }

  updatePushSubscription() {
    if (!this.props.target.registrationActor) {
      // A valid registrationActor is needed to retrieve the push subscription.
      return;
    }

    const { client, target } = this.props;
    client.request({
      to: target.registrationActor,
      type: "getPushSubscription"
    }, ({ subscription }) => {
      this.setState({ pushSubscription: subscription });
    });
  }

  isRunning() {
    // We know the target is running if it has a worker actor.
    return !!this.props.target.workerTargetActor;
  }

  isActive() {
    return this.props.target.active;
  }

  getServiceWorkerStatus() {
    if (this.isActive() && this.isRunning()) {
      return "running";
    } else if (this.isActive()) {
      return "stopped";
    }
    // We cannot get service worker registrations unless the registration is in
    // ACTIVE state. Unable to know the actual state ("installing", "waiting"), we
    // display a custom state "registering" for now. See Bug 1153292.
    return "registering";
  }

  renderButtons() {
    const pushButton = dom.button({
      className: "push-button",
      onClick: this.push,
      disabled: this.props.debugDisabled
    }, Strings.GetStringFromName("push"));

    const debugButton = dom.button({
      className: "debug-button",
      onClick: this.debug,
      disabled: this.props.debugDisabled
    }, Strings.GetStringFromName("debug"));

    const startButton = dom.button({
      className: "start-button",
      onClick: this.start,
      disabled: this.props.debugDisabled
    }, Strings.GetStringFromName("start"));

    if (this.isRunning()) {
      if (this.isActive()) {
        return [pushButton, debugButton];
      }
      // Only debug button is available if the service worker is not active.
      return debugButton;
    }
    return startButton;
  }

  renderUnregisterLink() {
    if (!this.isActive()) {
      // If not active, there might be no registrationActor available.
      return null;
    }

    return dom.a({
      onClick: this.unregister,
      className: "unregister-link",
    }, Strings.GetStringFromName("unregister"));
  }

  render() {
    const { target } = this.props;
    const { pushSubscription } = this.state;
    const status = this.getServiceWorkerStatus();

    const fetch = target.fetch ? Strings.GetStringFromName("listeningForFetchEvents") :
      Strings.GetStringFromName("notListeningForFetchEvents");

    return dom.div({ className: "target-container" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.span({ className: `target-status target-status-${status}` },
        Strings.GetStringFromName(status)),
      dom.div({ className: "target" },
        dom.div({ className: "target-name", title: target.name }, target.name),
        dom.ul({ className: "target-details" },
          (pushSubscription ?
            dom.li({ className: "target-detail" },
              dom.strong(null, Strings.GetStringFromName("pushService")),
              dom.span({
                className: "service-worker-push-url",
                title: pushSubscription.endpoint
              }, pushSubscription.endpoint)) :
            null
          ),
          dom.li({ className: "target-detail" },
            dom.strong(null, Strings.GetStringFromName("fetch")),
            dom.span({
              className: "service-worker-fetch-flag",
              title: fetch
            }, fetch)),
          dom.li({ className: "target-detail" },
            dom.strong(null, Strings.GetStringFromName("scope")),
            dom.span({
              className: "service-worker-scope",
              title: target.scope
            }, target.scope),
            this.renderUnregisterLink()
          )
        )
      ),
      this.renderButtons()
    );
  }
}

module.exports = ServiceWorkerTarget;
