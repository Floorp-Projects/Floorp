var Cu = Components.utils;
var Ci = Components.interfaces;

Cu.importGlobalProperties(['File']);

const { Services } = Cu.import("resource://gre/modules/Services.jsm");

// Load a duplicated copy of the jsm to prevent messing with the currently running one
var scope = {};
Services.scriptloader.loadSubScript("resource://gre/modules/Screenshot.jsm", scope);
const { Screenshot } = scope;

var index = -1;
function next() {
  index++;
  if (index >= steps.length) {
    assert.ok(false, "Shouldn't get here!");
    return;
  }
  try {
    steps[index]();
  } catch(ex) {
    assert.ok(false, "Caught exception: " + ex);
  }
}

var steps = [
  function getScreenshot() {
    let screenshot = Screenshot.get();
    assert.ok(screenshot instanceof File,
              "Screenshot.get() returns a File");
    next();
  },

  function endOfTest() {
    sendAsyncMessage("finish");
  }
];

next();
