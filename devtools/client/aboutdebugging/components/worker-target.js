/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals gDevTools, TargetFactory, Toolbox */

"use strict";

loader.lazyRequireGetter(this, "gDevTools",
  "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "TargetFactory",
  "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox",
  "devtools/client/framework/toolbox", true);

const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

module.exports = createClass({
  displayName: "WorkerTarget",

  render() {
    let { target, debugDisabled } = this.props;
    let isRunning = this.isRunning();
    let isServiceWorker = this.isServiceWorker();

    return dom.div({ className: "target" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.div({ className: "target-details" },
        dom.div({ className: "target-name" }, target.name)
      ),
      (isRunning && isServiceWorker ?
        dom.button({
          className: "push-button",
          onClick: this.push
        }, Strings.GetStringFromName("push")) :
        null
      ),
      (isRunning ?
        dom.button({
          className: "debug-button",
          onClick: this.debug,
          disabled: debugDisabled
        }, Strings.GetStringFromName("debug")) :
        dom.button({
          className: "start-button",
          onClick: this.start
        }, Strings.GetStringFromName("start"))
      )
    );
  },

  debug() {
    let { client, target } = this.props;
    if (!this.isRunning()) {
      // If the worker is not running, we can't debug it.
      return;
    }
    client.attachWorker(target.workerActor, (response, workerClient) => {
      let workerTarget = TargetFactory.forWorker(workerClient);
      gDevTools.showToolbox(workerTarget, "jsdebugger", Toolbox.HostType.WINDOW)
        .then(toolbox => {
          toolbox.once("destroy", () => workerClient.detach());
        });
    });
  },

  push() {
    let { client, target } = this.props;
    if (!this.isRunning()) {
      // If the worker is not running, we can't push to it.
      return;
    }
    client.request({
      to: target.workerActor,
      type: "push"
    });
  },

  start() {
    let { client, target } = this.props;
    if (!this.isServiceWorker() || this.isRunning()) {
      // Either the worker is already running, or it's not a service worker, in
      // which case we don't know how to start it.
      return;
    }
    client.request({
      to: target.registrationActor,
      type: "start"
    });
  },

  isRunning() {
    // We know the target is running if it has a worker actor.
    return !!this.props.target.workerActor;
  },

  isServiceWorker() {
    // We know the target is a service worker if it has a registration actor.
    return !!this.props.target.registrationActor;
  },
});
