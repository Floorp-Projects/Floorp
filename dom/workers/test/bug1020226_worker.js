var p = new Promise(function(resolve, reject) {
  // This causes a runnable to be queued.
  reject(new Error());
  postMessage("loaded");

  // This prevents that runnable from running until the window calls terminate(),
  // at which point the worker goes into the Canceling state and then an
  // AddFeature() is attempted, which fails, which used to result in multiple
  // calls to the error reporter, one after the worker's context had been GCed.
  while (true);
});
