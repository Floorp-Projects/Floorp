let { classes: Cc, interfaces: Ci, utils: Cu } = Components;


let swm = Cc["@mozilla.org/serviceworkers/manager;1"].
          getService(Ci.nsIServiceWorkerManager);

let EXAMPLE_URL = "https://example.com/chrome/dom/workers/test/serviceworkers/";

function waitForIframeLoad(iframe) {
  return new Promise(function (resolve) {
    iframe.onload = resolve;
  });
}

function waitForRegister(scope, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onRegister: function (registration) {
        if (registration.scope !== scope) {
          return;
        }
        swm.removeListener(listener);
        resolve(callback ? callback(registration) : registration);
      }
    };
    swm.addListener(listener);
  });
}

function waitForUnregister(scope) {
  return new Promise(function (resolve) {
    let listener = {
      onUnregister: function (registration) {
        if (registration.scope !== scope) {
          return;
        }
        swm.removeListener(listener);
        resolve(registration);
      }
    };
    swm.addListener(listener);
  });
}

function waitForServiceWorkerRegistrationChange(registration, callback) {
  return new Promise(function (resolve) {
    let listener = {
      onChange: function () {
        registration.removeListener(listener);
        if (callback) {
          callback();
        }
        resolve(callback ? callback() : undefined);
      }
    };
    registration.addListener(listener);
  });
}

function waitForServiceWorkerShutdown() {
  return new Promise(function (resolve) {
    let observer = {
      observe: function (subject, topic, data) {
        if (topic !== "service-worker-shutdown") {
          return;
        }
        SpecialPowers.removeObserver(observer, "service-worker-shutdown");
        resolve();
      }
    };
    SpecialPowers.addObserver(observer, "service-worker-shutdown");
  });
}
