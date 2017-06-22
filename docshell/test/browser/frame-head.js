/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Functions that are automatically loaded as frame scripts for
// timeline tests.

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { Promise } = Cu.import('resource://gre/modules/Promise.jsm', {});

Cu.import("resource://gre/modules/Timer.jsm");

// Functions that look like mochitest functions but forward to the
// browser process.

this.ok = function(value, message) {
  sendAsyncMessage("browser:test:ok", {
    value: !!value,
    message: message});
}

this.is = function(v1, v2, message) {
  ok(v1 == v2, message);
}

this.info = function(message) {
  sendAsyncMessage("browser:test:info", {message: message});
}

this.finish = function() {
  sendAsyncMessage("browser:test:finish");
}

/* Start a task that runs some timeline tests in the ordinary way.
 *
 * @param array tests
 *        The tests to run.  This is an array where each element
 *        is of the form { desc, searchFor, setup, check }.
 *
 *        desc is the test description, a string.
 *        searchFor is a string or a function
 *             If a string, then when a marker with this name is
 *             found, marker-reading is stopped.
 *             If a function, then the accumulated marker array is
 *             passed to it, and marker reading stops when it returns
 *             true.
 *        setup is a function that takes the docshell as an argument.
 *             It should start the test.
 *        check is a function that takes an array of markers
 *             as an argument and checks the results of the test.
 */
this.timelineContentTest = function(tests) {
  (async function() {
    let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);

    info("Start recording");
    docShell.recordProfileTimelineMarkers = true;

    for (let {desc, searchFor, setup, check} of tests) {

      info("Running test: " + desc);

      info("Flushing the previous markers if any");
      docShell.popProfileTimelineMarkers();

      info("Running the test setup function");
      let onMarkers = timelineWaitForMarkers(docShell, searchFor);
      setup(docShell);
      info("Waiting for new markers on the docShell");
      let markers = await onMarkers;

      // Cycle collection markers are non-deterministic, and none of these tests
      // expect them to show up.
      markers = markers.filter(m => m.name.indexOf("nsCycleCollector") === -1);

      info("Running the test check function");
      check(markers);
    }

    info("Stop recording");
    docShell.recordProfileTimelineMarkers = false;
    finish();
  })();
}

function timelineWaitForMarkers(docshell, searchFor) {
  if (typeof(searchFor) == "string") {
    let searchForString = searchFor;
    let f = function (markers) {
      return markers.some(m => m.name == searchForString);
    };
    searchFor = f;
  }

  return new Promise(function(resolve, reject) {
    let waitIterationCount = 0;
    let maxWaitIterationCount = 10; // Wait for 2sec maximum
    let markers = [];

    setTimeout(function timeoutHandler() {
      let newMarkers = docshell.popProfileTimelineMarkers();
      markers = [...markers, ...newMarkers];
      if (searchFor(markers) || waitIterationCount > maxWaitIterationCount) {
        resolve(markers);
      } else {
        setTimeout(timeoutHandler, 200);
        waitIterationCount++;
      }
    }, 200);
  });
}
