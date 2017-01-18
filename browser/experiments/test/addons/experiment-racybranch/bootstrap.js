/* exported startup, shutdown, install, uninstall */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/experiments/Experiments.jsm");

var gStarted = false;

function startup(data, reasonCode) {
  if (gStarted) {
    return;
  }
  gStarted = true;

  // delay realstartup to trigger the race condition
  Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager)
    .mainThread.dispatch(realstartup, 0);
}

function realstartup() {
  let experiments = Experiments.instance();
  let experiment = experiments._getActiveExperiment();
  if (experiment.branch) {
    Cu.reportError("Found pre-existing branch: " + experiment.branch);
    return;
  }

  let branch = "racy-set";
  experiments.setExperimentBranch(experiment.id, branch)
    .then(null, Cu.reportError);
}

function shutdown() { }
function install() { }
function uninstall() { }
