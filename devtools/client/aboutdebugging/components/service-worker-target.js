/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const { debugWorker } = require("../modules/worker");
const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

module.exports = createClass({
  displayName: "ServiceWorkerTarget",

  debug() {
    if (!this.isRunning()) {
      // If the worker is not running, we can't debug it.
      return;
    }

    let { client, target } = this.props;
    debugWorker(client, target.workerActor);
  },

  push() {
    if (!this.isRunning()) {
      // If the worker is not running, we can't push to it.
      return;
    }

    let { client, target } = this.props;
    client.request({
      to: target.workerActor,
      type: "push"
    });
  },

  start() {
    if (this.isRunning()) {
      // If the worker is already running, we can't start it.
      return;
    }

    let { client, target } = this.props;
    client.request({
      to: target.registrationActor,
      type: "start"
    });
  },

  unregister() {
    let { client, target } = this.props;
    client.request({
      to: target.registrationActor,
      type: "unregister"
    });
  },

  isRunning() {
    // We know the target is running if it has a worker actor.
    return !!this.props.target.workerActor;
  },

  render() {
    let { target, debugDisabled } = this.props;
    let isRunning = this.isRunning();

    return dom.div({ className: "target-container" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.div({ className: "target" },
        dom.div({ className: "target-name" }, target.name),
        dom.ul({ className: "target-details" },
          dom.li({ className: "target-detail" },
            dom.strong(null, Strings.GetStringFromName("scope")),
            dom.span({ className: "service-worker-scope" }, target.scope),
            dom.a({
              onClick: this.unregister,
              className: "unregister-link"
            }, Strings.GetStringFromName("unregister"))
          )
        )
      ),
      (isRunning ?
        [
          dom.button({
            className: "push-button",
            onClick: this.push
          }, Strings.GetStringFromName("push")),
          dom.button({
            className: "debug-button",
            onClick: this.debug,
            disabled: debugDisabled
          }, Strings.GetStringFromName("debug"))
        ] :
        dom.button({
          className: "start-button",
          onClick: this.start
        }, Strings.GetStringFromName("start"))
      )
    );
  }
});
