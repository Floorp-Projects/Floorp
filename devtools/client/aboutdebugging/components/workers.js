/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React, TargetListComponent */

"use strict";

loader.lazyRequireGetter(this, "Ci",
  "chrome", true);
loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");
loader.lazyRequireGetter(this, "TargetListComponent",
  "devtools/client/aboutdebugging/components/target-list", true);
loader.lazyRequireGetter(this, "Services");

loader.lazyImporter(this, "Task", "resource://gre/modules/Task.jsm");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");
const WorkerIcon = "chrome://devtools/skin/images/debugging-workers.svg";

exports.WorkersComponent = React.createClass({
  displayName: "WorkersComponent",

  getInitialState() {
    return {
      workers: {
        service: [],
        shared: [],
        other: []
      }
    };
  },

  componentDidMount() {
    let client = this.props.client;
    client.addListener("workerListChanged", this.update);
    client.addListener("processListChanged", this.update);
    this.update();
  },

  componentWillUnmount() {
    let client = this.props.client;
    client.removeListener("processListChanged", this.update);
    client.removeListener("workerListChanged", this.update);
  },

  render() {
    let client = this.props.client;
    let workers = this.state.workers;
    return React.createElement("div", { className: "inverted-icons" },
      React.createElement(TargetListComponent, {
        id: "service-workers",
        name: Strings.GetStringFromName("serviceWorkers"),
        targets: workers.service, client }),
      React.createElement(TargetListComponent, {
        id: "shared-workers",
        name: Strings.GetStringFromName("sharedWorkers"),
        targets: workers.shared, client }),
      React.createElement(TargetListComponent, {
        id: "other-workers",
        name: Strings.GetStringFromName("otherWorkers"),
        targets: workers.other, client })
    );
  },

  update() {
    let workers = this.getInitialState().workers;
    this.getWorkerForms().then(forms => {
      forms.forEach(form => {
        let worker = {
          name: form.url,
          icon: WorkerIcon,
          actorID: form.actor
        };
        switch (form.type) {
          case Ci.nsIWorkerDebugger.TYPE_SERVICE:
            worker.type = "serviceworker";
            workers.service.push(worker);
            break;
          case Ci.nsIWorkerDebugger.TYPE_SHARED:
            worker.type = "sharedworker";
            workers.shared.push(worker);
            break;
          default:
            worker.type = "worker";
            workers.other.push(worker);
        }
      });
      this.setState({ workers });
    });
  },

  getWorkerForms: Task.async(function*() {
    let client = this.props.client;

    // List workers from the Parent process
    let result = yield client.mainRoot.listWorkers();
    let forms = result.workers;

    // And then from the Child processes
    let { processes } = yield client.mainRoot.listProcesses();
    for (let process of processes) {
      // Ignore parent process
      if (process.parent) {
        continue;
      }
      let { form } = yield client.getProcess(process.id);
      let processActor = form.actor;
      let { workers } = yield client.request({to: processActor,
                                              type: "listWorkers"});
      forms = forms.concat(workers);
    }

    return forms;
  }),
});
