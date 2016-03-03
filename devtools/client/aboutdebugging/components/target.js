/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

loader.lazyRequireGetter(this, "TargetFactory",
      "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "gDevTools",
      "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "Toolbox",
      "devtools/client/framework/toolbox", true);

loader.lazyImporter(this, "BrowserToolboxProcess",
      "resource://devtools/client/framework/ToolboxProcess.jsm");

const Services = require("Services");
const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

module.exports = createClass({
  displayName: "Target",

  render() {
    let { target, debugDisabled } = this.props;
    let isServiceWorker = (target.type === "serviceworker");
    let isRunning = (!isServiceWorker || target.workerActor);
    return dom.div({ className: "target" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon }),
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
          disabled: debugDisabled,
        }, Strings.GetStringFromName("debug")) :
        dom.button({
          className: "start-button",
          onClick: this.start
        }, Strings.GetStringFromName("start"))
      )
    );
  },

  debug() {
    let { target } = this.props;
    switch (target.type) {
      case "extension":
        BrowserToolboxProcess.init({ addonID: target.addonID });
        break;
      case "serviceworker":
        if (target.workerActor) {
          this.openWorkerToolbox(target.workerActor);
        }
        break;
      case "sharedworker":
        this.openWorkerToolbox(target.workerActor);
        break;
      case "worker":
        this.openWorkerToolbox(target.workerActor);
        break;
      default:
        window.alert("Not implemented yet!");
        break;
    }
  },

  push() {
    let { client, target } = this.props;
    if (target.workerActor) {
      client.request({
        to: target.workerActor,
        type: "push"
      });
    }
  },

  start() {
    let { client, target } = this.props;
    if (target.type === "serviceworker" && !target.workerActor) {
      client.request({
        to: target.registrationActor,
        type: "start"
      });
    }
  },

  openWorkerToolbox(workerActor) {
    let { client } = this.props;
    client.attachWorker(workerActor, (response, workerClient) => {
      gDevTools.showToolbox(TargetFactory.forWorker(workerClient),
        "jsdebugger", Toolbox.HostType.WINDOW)
        .then(toolbox => {
          toolbox.once("destroy", () => workerClient.detach());
        });
    });
  },
});
