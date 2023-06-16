let swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
  Ci.nsIServiceWorkerManager
);

let EXAMPLE_URL = "https://example.com/chrome/dom/serviceworkers/test/";

function waitForIframeLoad(iframe) {
  return new Promise(function (resolve) {
    iframe.onload = resolve;
  });
}

function waitForRegister(scope, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onRegister(registration) {
        if (registration.scope !== scope) {
          return;
        }
        swm.removeListener(listener);
        resolve(callback ? callback(registration) : registration);
      },
    };
    swm.addListener(listener);
  });
}

function waitForUnregister(scope) {
  return new Promise(function (resolve) {
    let listener = {
      onUnregister(registration) {
        if (registration.scope !== scope) {
          return;
        }
        swm.removeListener(listener);
        resolve(registration);
      },
    };
    swm.addListener(listener);
  });
}

function waitForServiceWorkerRegistrationChange(registration, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onChange() {
        registration.removeListener(listener);
        if (callback) {
          callback();
        }
        resolve(callback ? callback() : undefined);
      },
    };
    registration.addListener(listener);
  });
}

function waitForServiceWorkerShutdown() {
  return new Promise(function (resolve) {
    let observer = {
      observe(subject, topic, data) {
        if (topic !== "service-worker-shutdown") {
          return;
        }
        SpecialPowers.removeObserver(observer, "service-worker-shutdown");
        resolve();
      },
    };
    SpecialPowers.addObserver(observer, "service-worker-shutdown");
  });
}
