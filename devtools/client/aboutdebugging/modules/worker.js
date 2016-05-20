/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("devtools/shared/task");

loader.lazyRequireGetter(this, "gDevTools",
  "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "TargetFactory",
  "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox",
  "devtools/client/framework/toolbox", true);

/**
 * Open a window-hosted toolbox to debug the worker associated to the provided
 * worker actor.
 *
 * @param {DebuggerClient} client
 * @param {Object} workerActor
 *        worker actor form to debug
 */
exports.debugWorker = function (client, workerActor) {
  client.attachWorker(workerActor, (response, workerClient) => {
    let workerTarget = TargetFactory.forWorker(workerClient);
    gDevTools.showToolbox(workerTarget, "jsdebugger", Toolbox.HostType.WINDOW)
      .then(toolbox => {
        toolbox.once("destroy", () => workerClient.detach());
      });
  });
};

/**
 * Retrieve all service worker registrations as well as workers from the parent
 * and child processes.
 *
 * @param {DebuggerClient} client
 * @return {Object}
 *         - {Array} registrations
 *           Array of ServiceWorkerRegistrationActor forms
 *         - {Array} workers
 *           Array of WorkerActor forms
 */
exports.getWorkerForms = Task.async(function* (client) {
  let registrations = [];
  let workers = [];

  try {
    // List service worker registrations
    ({ registrations } =
      yield client.mainRoot.listServiceWorkerRegistrations());

    // List workers from the Parent process
    ({ workers } = yield client.mainRoot.listWorkers());

    // And then from the Child processes
    let { processes } = yield client.mainRoot.listProcesses();
    for (let process of processes) {
      // Ignore parent process
      if (process.parent) {
        continue;
      }
      let { form } = yield client.getProcess(process.id);
      let processActor = form.actor;
      let response = yield client.request({
        to: processActor,
        type: "listWorkers"
      });
      workers = workers.concat(response.workers);
    }
  } catch (e) {
    // Something went wrong, maybe our client is disconnected?
  }

  return { registrations, workers };
});
