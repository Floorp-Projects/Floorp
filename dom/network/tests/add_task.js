// Temporary implementation of add_task for mochitest-plain until bug 1078657 is
// implemented.
SimpleTest.waitForExplicitFinish();
(function(scope) {
  var pendingTasks = [];
  var pendingPromise = null;

  // Strict spawn function that takes a known generatorFunc and assumes that
  // every yielded value will be a Promise.  If nesting is desired, then yield*
  // should be used!
  function spawn(generatorFunc) {
    return new Promise(function(resolve, reject) {
      try {
        var iterator = generatorFunc();
      }
      catch (ex) {
        ok(false, 'Problem invoking generator func: ' + ex + ': ' + ex.stack);
        return;
      }
      var stepResolved = function(result) {
        try {
          var iterStep = iterator.next(result);
        }
        catch (ex) {
          ok(false, 'Problem invoking iterator step: ' + ex + ': ' + ex.stack);
          return;
        }
        if (iterStep.done) {
          resolve(iterStep.value);
          return;
        }
        if (!iterStep.value || !iterStep.value.then) {
          ok(false, 'Iterator step returned non-Promise: ' + iterStep.value);
        }
        iterStep.value.then(stepResolved, generalErrback);
      };
      stepResolved();
    });
  }

  function maybeSpawn(promiseOrGenerator) {
    if (promiseOrGenerator.then) {
      return promiseOrGenerator;
    }
    return spawn(promiseOrGenerator);
  }

  scope.add_task = function(thing) {
    pendingTasks.push(thing);
  };

  function generalErrback(ex) {
    ok(false,
       'A rejection happened: ' +
       (ex ? (ex + ': ' + ex.stack) : ''));
  }

  function runNextTask() {
    if (pendingTasks.length) {
      pendingPromise = maybeSpawn(pendingTasks.shift());
      pendingPromise.then(runNextTask, generalErrback);
    } else {
      SimpleTest.finish();
    }
  }

  // Trigger runNextTask after we think all JS files have been loaded.
  // The primary goal is that we can call SimpleTest.finish() after all test
  // code has been loaded and run.  We gate this based on the document's
  // readyState.
  var running = false;
  function maybeStartRunning() {
    if (!running && document.readyState === 'complete') {
      running = true;
      document.removeEventListener('readystateChange', maybeStartRunning);
      // Defer to a subsequent turn of the event loop to let micro-tasks and any
      // other clever setTimeout(0) instances run first.
      window.setTimeout(runNextTask, 0);
    }
  }
  document.addEventListener('readystatechange', maybeStartRunning);
  maybeStartRunning();
})(this);
