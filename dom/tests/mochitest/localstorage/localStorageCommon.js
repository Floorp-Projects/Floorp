function localStorageFlush(cb) {
  if (SpecialPowers.Services.lsm.nextGenLocalStorageEnabled) {
    SimpleTest.executeSoon(function() {
      cb();
    });
    return;
  }

  var ob = {
    observe: function(sub, top, dat) {
      os().removeObserver(ob, "domstorage-test-flushed");
      cb();
    },
  };
  os().addObserver(ob, "domstorage-test-flushed");
  notify("domstorage-test-flush-force");
}

function localStorageReload(callback) {
  if (SpecialPowers.Services.lsm.nextGenLocalStorageEnabled) {
    localStorage.close();
    let qms = SpecialPowers.Services.qms;
    let principal = SpecialPowers.wrap(document).nodePrincipal;
    let request = qms.resetStoragesForPrincipal(principal, "default", "ls");
    request.callback = SpecialPowers.wrapCallback(function() {
      localStorage.open();
      callback();
    });
    return;
  }

  notify("domstorage-test-reload");
  SimpleTest.executeSoon(function() {
    callback();
  });
}

function localStorageFlushAndReload(callback) {
  if (SpecialPowers.Services.lsm.nextGenLocalStorageEnabled) {
    localStorage.close();
    let qms = SpecialPowers.Services.qms;
    let principal = SpecialPowers.wrap(document).nodePrincipal;
    let request = qms.resetStoragesForPrincipal(principal, "default", "ls");
    request.callback = SpecialPowers.wrapCallback(function() {
      localStorage.open();
      callback();
    });
    return;
  }

  localStorageFlush(function() {
    localStorageReload(callback);
  });
}

function localStorageClearAll(callback) {
  if (SpecialPowers.Services.lsm.nextGenLocalStorageEnabled) {
    let qms = SpecialPowers.Services.qms;
    let ssm = SpecialPowers.Services.scriptSecurityManager;

    qms.getUsage(
      SpecialPowers.wrapCallback(function(request) {
        if (request.resultCode != SpecialPowers.Cr.NS_OK) {
          callback();
          return;
        }

        let clearRequestCount = 0;
        for (let item of request.result) {
          let principal = ssm.createCodebasePrincipalFromOrigin(item.origin);
          let clearRequest = qms.clearStoragesForPrincipal(
            principal,
            "default",
            "ls"
          );
          clearRequestCount++;
          clearRequest.callback = SpecialPowers.wrapCallback(function() {
            if (--clearRequestCount == 0) {
              callback();
            }
          });
        }
      })
    );
    return;
  }

  os().notifyObservers(null, "cookie-changed", "cleared");
  SimpleTest.executeSoon(function() {
    callback();
  });
}

function localStorageClearDomain(domain, callback) {
  if (SpecialPowers.Services.lsm.nextGenLocalStorageEnabled) {
    let qms = SpecialPowers.Services.qms;
    let principal = SpecialPowers.wrap(document).nodePrincipal;
    let request = qms.clearStoragesForPrincipal(principal, "default", "ls");
    let cb = SpecialPowers.wrapCallback(callback);
    request.callback = cb;
    return;
  }

  os().notifyObservers(null, "extension:purge-localStorage", domain);
  SimpleTest.executeSoon(function() {
    callback();
  });
}

function os() {
  return SpecialPowers.Services.obs;
}

function notify(top) {
  os().notifyObservers(null, top);
}

/**
 * Enable testing observer notifications in DOMStorageObserver.cpp.
 */
function localStorageEnableTestingMode(cb) {
  SpecialPowers.pushPrefEnv(
    {
      set: [["dom.storage.testing", true], ["dom.quotaManager.testing", true]],
    },
    cb
  );
}
