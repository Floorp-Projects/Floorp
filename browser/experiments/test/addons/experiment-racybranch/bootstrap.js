/* exported startup, shutdown, install, uninstall */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/experiments/Experiments.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var gStarted = false;

function startup(data, reasonCode) {
  if (gStarted) {
    return;
  }
  gStarted = true;

  // delay realstartup to trigger the race condition
  Services.tm.dispatchToMainThread(realstartup);
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
    .catch(Cu.reportError);
}

function shutdown() { }
function install() { }
function uninstall() { }
