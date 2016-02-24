/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global alert, BrowserToolboxProcess, gDevTools, React, TargetFactory,
   Toolbox */

"use strict";

loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");
loader.lazyRequireGetter(this, "TargetFactory",
  "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox",
  "devtools/client/framework/toolbox", true);
loader.lazyRequireGetter(this, "Services");

loader.lazyImporter(this, "BrowserToolboxProcess",
  "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyRequireGetter(this, "gDevTools",
  "devtools/client/framework/devtools", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

exports.Target = React.createClass({
  displayName: "Target",

  render() {
    let { target, debugDisabled } = this.props;
    let isServiceWorker = (target.type === "serviceworker");
    let isRunning = (!isServiceWorker || target.workerActor);
    return React.createElement("div", { className: "target" },
      React.createElement("img", {
        className: "target-icon",
        role: "presentation",
        src: target.icon }),
      React.createElement("div", { className: "target-details" },
        React.createElement("div", { className: "target-name" }, target.name)
      ),
      (isRunning ?
        React.createElement("button", {
          className: "debug-button",
          onClick: this.debug,
          disabled: debugDisabled,
        }, Strings.GetStringFromName("debug")) :
        null
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
        alert("Not implemented yet!");
        break;
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
