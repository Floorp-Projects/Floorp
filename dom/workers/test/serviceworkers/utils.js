function waitForState(worker, state, context) {
  return new Promise(resolve => {
    if (worker.state === state) {
      resolve(context);
      return;
    }
    worker.addEventListener('statechange', function onStateChange() {
      if (worker.state === state) {
        worker.removeEventListener('statechange', onStateChange);
        resolve(context);
      }
    });
  });
}

function waitForControlled(win) {
  return new Promise(resolve => {
    if (win.navigator.serviceWorker.controller) {
      return resolve();
    }

    win.navigator.serviceWorker.addEventListener('controllerchange', resolve,
                                                 { once: true });
  });
}
