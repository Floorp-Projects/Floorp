/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

importScripts("onLine_worker_head.js");

var N_CHILDREN = 3;
var children = [];
var finishedChildrenCount = 0;
var lastTest = false;

for (var event of ["online", "offline"]) {
  addEventListener(event,
                   makeHandler(
                     "addEventListener('%1', ..., false)",
                     event, 1, "Parent Worker"),
                   false);
}

onmessage = function(e) {
  if (e.data === 'lastTest') {
    children.forEach(function(w) {
      w.postMessage({ type: 'lastTest' });
    });
    lastTest = true;
  }
}

function setupChildren(cb) {
  var readyCount = 0;
  for (var i = 0; i < N_CHILDREN; ++i) {
    var w = new Worker("onLine_worker_child.js");
    children.push(w);

    w.onerror = function(e) {
      info("Error creating child " + e.message);
    }

    w.onmessage = function(e) {
      if (e.data.type === 'ready') {
        info("Got ready from child");
        readyCount++;
        if (readyCount === N_CHILDREN) {
          cb();
        }
      } else if (e.data.type === 'finished') {
        finishedChildrenCount++;

        if (lastTest && finishedChildrenCount === N_CHILDREN) {
          postMessage({ type: 'finished' });
          children = [];
          close();
        }
      } else if (e.data.type === 'ok') {
        // Pass on test to page.
        postMessage(e.data);
      }
    }
  }
}

setupChildren(function() {
  postMessage({ type: 'ready' });
});
