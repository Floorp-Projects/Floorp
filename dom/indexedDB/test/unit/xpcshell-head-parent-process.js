/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests using testGenerator are expected to define it themselves.
// Testing functions are expected to call testSteps and its type should either
// be GeneratorFunction or AsyncFunction
/* global testGenerator, testSteps:false */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

if (!("self" in this)) {
  this.self = this;
}

var bufferCache = [];

function is(a, b, msg) {
  Assert.equal(a, b, msg);
}

function ok(cond, msg) {
  Assert.ok(!!cond, msg);
}

function isnot(a, b, msg) {
  Assert.notEqual(a, b, msg);
}

function todo(condition, name, diag) {
  todo_check_true(condition);
}

function run_test() {
  runTest();
}

if (!this.runTest) {
  this.runTest = function() {
    if (SpecialPowers.isMainProcess()) {
      // XPCShell does not get a profile by default.
      do_get_profile();

      enableTesting();
      enableExperimental();
    }

    Cu.importGlobalProperties(["indexedDB"]);

    // In order to support converting tests to using async functions from using
    // generator functions, we detect async functions by checking the name of
    // function's constructor.
    Assert.ok(
      typeof testSteps === "function",
      "There should be a testSteps function"
    );
    if (testSteps.constructor.name === "AsyncFunction") {
      // Do run our existing cleanup function that would normally be called by
      // the generator's call to finishTest().
      registerCleanupFunction(resetTesting);

      add_task(testSteps);

      // Since we defined run_test, we must invoke run_next_test() to start the
      // async test.
      run_next_test();
    } else {
      Assert.ok(
        testSteps.constructor.name === "GeneratorFunction",
        "Unsupported function type"
      );

      do_test_pending();
      testGenerator.next();
    }
  };
}

function finishTest() {
  if (SpecialPowers.isMainProcess()) {
    resetExperimental();
    resetTesting();
  }

  SpecialPowers.removeFiles();

  executeSoon(function() {
    do_test_finished();
  });
}

function grabEventAndContinueHandler(event) {
  testGenerator.next(event);
}

function continueToNextStep() {
  executeSoon(function() {
    testGenerator.next();
  });
}

function errorHandler(event) {
  try {
    dump("indexedDB error: " + event.target.error.name);
  } catch (e) {
    dump("indexedDB error: " + e);
  }
  Assert.ok(false);
  finishTest();
}

function unexpectedSuccessHandler() {
  Assert.ok(false);
  finishTest();
}

function expectedErrorHandler(name) {
  return function(event) {
    Assert.equal(event.type, "error");
    Assert.equal(event.target.error.name, name);
    event.preventDefault();
    grabEventAndContinueHandler(event);
  };
}

function expectUncaughtException(expecting) {
  // This is dummy for xpcshell test.
}

function ExpectError(name, preventDefault) {
  this._name = name;
  this._preventDefault = preventDefault;
}
ExpectError.prototype = {
  handleEvent(event) {
    Assert.equal(event.type, "error");
    Assert.equal(this._name, event.target.error.name);
    if (this._preventDefault) {
      event.preventDefault();
      event.stopPropagation();
    }
    grabEventAndContinueHandler(event);
  },
};

function continueToNextStepSync() {
  testGenerator.next();
}

// TODO compareKeys is duplicated in ../helpers.js, can we import that here?
// the same applies to many other functions in this file
// this duplication should be avoided (bug 1565986)
function compareKeys(k1, k2) {
  let t = typeof k1;
  if (t != typeof k2) {
    return false;
  }

  if (t !== "object") {
    return k1 === k2;
  }

  if (k1 instanceof Date) {
    return k2 instanceof Date && k1.getTime() === k2.getTime();
  }

  if (k1 instanceof Array) {
    if (!(k2 instanceof Array) || k1.length != k2.length) {
      return false;
    }

    for (let i = 0; i < k1.length; ++i) {
      if (!compareKeys(k1[i], k2[i])) {
        return false;
      }
    }

    return true;
  }

  if (k1 instanceof ArrayBuffer) {
    if (!(k2 instanceof ArrayBuffer)) {
      return false;
    }

    function arrayBuffersAreEqual(a, b) {
      if (a.byteLength != b.byteLength) {
        return false;
      }
      let ui8b = new Uint8Array(b);
      return new Uint8Array(a).every((val, i) => val === ui8b[i]);
    }

    return arrayBuffersAreEqual(k1, k2);
  }

  return false;
}

function addPermission(permission, url) {
  throw new Error("addPermission");
}

function removePermission(permission, url) {
  throw new Error("removePermission");
}

function allowIndexedDB(url) {
  throw new Error("allowIndexedDB");
}

function disallowIndexedDB(url) {
  throw new Error("disallowIndexedDB");
}

function enableExperimental() {
  SpecialPowers.setBoolPref("dom.indexedDB.experimental", true);
}

function resetExperimental() {
  SpecialPowers.clearUserPref("dom.indexedDB.experimental");
}

function enableTesting() {
  SpecialPowers.setBoolPref("dom.indexedDB.testing", true);
}

function resetTesting() {
  SpecialPowers.clearUserPref("dom.indexedDB.testing");
}

function gc() {
  Cu.forceGC();
  Cu.forceCC();
}

function scheduleGC() {
  SpecialPowers.exactGC(continueToNextStep);
}

function setTimeout(fun, timeout) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  var event = {
    notify(timer) {
      fun();
    },
  };
  timer.initWithCallback(event, timeout, Ci.nsITimer.TYPE_ONE_SHOT);
  return timer;
}

function initStorageAndChromeOrigin(persistence) {
  let principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  return Services.qms.initStorageAndOrigin(principal, persistence, "idb");
}

function resetOrClearAllDatabases(callback, clear) {
  if (!SpecialPowers.isMainProcess()) {
    throw new Error("clearAllDatabases not implemented for child processes!");
  }

  const quotaPref = "dom.quotaManager.testing";

  let oldPrefValue;
  if (Services.prefs.prefHasUserValue(quotaPref)) {
    oldPrefValue = SpecialPowers.getBoolPref(quotaPref);
  }

  SpecialPowers.setBoolPref(quotaPref, true);

  let request;

  try {
    if (clear) {
      request = Services.qms.clear();
    } else {
      request = Services.qms.reset();
    }
  } catch (e) {
    if (oldPrefValue !== undefined) {
      SpecialPowers.setBoolPref(quotaPref, oldPrefValue);
    } else {
      SpecialPowers.clearUserPref(quotaPref);
    }
    throw e;
  }

  request.callback = callback;

  return request;
}

function resetAllDatabases(callback) {
  return resetOrClearAllDatabases(callback, false);
}

function clearAllDatabases(callback) {
  return resetOrClearAllDatabases(callback, true);
}

function installPackagedProfile(packageName) {
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  let packageFile = currentDir.clone();
  packageFile.append(packageName + ".zip");

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipReader.open(packageFile);

  let entryNames = [];
  for (let entry of zipReader.findEntries(null)) {
    if (entry != "create_db.html") {
      entryNames.push(entry);
    }
  }
  entryNames.sort();

  for (let entryName of entryNames) {
    let zipentry = zipReader.getEntry(entryName);

    let file = profileDir.clone();
    let split = entryName.split("/");
    for (let i = 0; i < split.length; i++) {
      file.append(split[i]);
    }

    if (zipentry.isDirectory) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
    } else {
      let istream = zipReader.getInputStream(entryName);

      var ostream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      ostream.init(file, -1, parseInt("0644", 8), 0);

      let bostream = Cc[
        "@mozilla.org/network/buffered-output-stream;1"
      ].createInstance(Ci.nsIBufferedOutputStream);
      bostream.init(ostream, 32768);

      bostream.writeFrom(istream, istream.available());

      istream.close();
      bostream.close();
    }
  }

  zipReader.close();
}

function getChromeFilesDir() {
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  let idbDir = profileDir.clone();
  idbDir.append("storage");
  idbDir.append("permanent");
  idbDir.append("chrome");
  idbDir.append("idb");

  let idbEntries = idbDir.directoryEntries;
  while (idbEntries.hasMoreElements()) {
    let file = idbEntries.nextFile;
    if (file.isDirectory()) {
      return file;
    }
  }

  throw new Error("files directory doesn't exist!");
}

function getView(size) {
  let buffer = new ArrayBuffer(size);
  let view = new Uint8Array(buffer);
  is(buffer.byteLength, size, "Correct byte length");
  return view;
}

function getRandomView(size) {
  let view = getView(size);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255);
  }
  return view;
}

function getBlob(str) {
  return new Blob([str], { type: "type/text" });
}

function getFile(name, type, str) {
  return new File([str], name, { type });
}

function isWasmSupported() {
  let testingFunctions = Cu.getJSTestingFunctions();
  return testingFunctions.wasmIsSupported();
}

function getWasmModule(binary) {
  let module = new WebAssembly.Module(binary);
  return module;
}

function compareBuffers(buffer1, buffer2) {
  if (buffer1.byteLength != buffer2.byteLength) {
    return false;
  }

  let view1 = buffer1 instanceof Uint8Array ? buffer1 : new Uint8Array(buffer1);
  let view2 = buffer2 instanceof Uint8Array ? buffer2 : new Uint8Array(buffer2);
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1[i] != view2[i]) {
      return false;
    }
  }
  return true;
}

function verifyBuffers(buffer1, buffer2) {
  ok(compareBuffers(buffer1, buffer2), "Correct buffer data");
}

function verifyBlob(blob1, blob2) {
  is(Blob.isInstance(blob1), true, "Instance of nsIDOMBlob");
  is(blob1 instanceof File, blob2 instanceof File, "Instance of DOM File");
  is(blob1.size, blob2.size, "Correct size");
  is(blob1.type, blob2.type, "Correct type");
  if (blob2 instanceof File) {
    is(blob1.name, blob2.name, "Correct name");
  }

  let buffer1;
  let buffer2;

  for (let i = 0; i < bufferCache.length; i++) {
    if (bufferCache[i].blob == blob2) {
      buffer2 = bufferCache[i].buffer;
      break;
    }
  }

  if (!buffer2) {
    let reader = new FileReader();
    reader.readAsArrayBuffer(blob2);
    reader.onload = function(event) {
      buffer2 = event.target.result;
      bufferCache.push({ blob: blob2, buffer: buffer2 });
      if (buffer1) {
        verifyBuffers(buffer1, buffer2);
        testGenerator.next();
      }
    };
  }

  let reader = new FileReader();
  reader.readAsArrayBuffer(blob1);
  reader.onload = function(event) {
    buffer1 = event.target.result;
    if (buffer2) {
      verifyBuffers(buffer1, buffer2);
      testGenerator.next();
    }
  };
}

function verifyMutableFile(mutableFile1, file2) {
  is(
    mutableFile1 instanceof IDBMutableFile,
    true,
    "Instance of IDBMutableFile"
  );
  is(mutableFile1.name, file2.name, "Correct name");
  is(mutableFile1.type, file2.type, "Correct type");
  continueToNextStep();
}

function verifyView(view1, view2) {
  is(view1.byteLength, view2.byteLength, "Correct byteLength");
  verifyBuffers(view1, view2);
  continueToNextStep();
}

function grabFileUsageAndContinueHandler(request) {
  testGenerator.next(request.result.fileUsage);
}

function getCurrentUsage(usageHandler) {
  let principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  Services.qms.getUsageForPrincipal(principal, usageHandler);
}

function setTemporaryStorageLimit(limit) {
  const pref = "dom.quotaManager.temporaryStorage.fixedLimit";
  if (limit) {
    info("Setting temporary storage limit to " + limit);
    SpecialPowers.setIntPref(pref, limit);
  } else {
    info("Removing temporary storage limit");
    SpecialPowers.clearUserPref(pref);
  }
}

function setDataThreshold(threshold) {
  info("Setting data threshold to " + threshold);
  SpecialPowers.setIntPref("dom.indexedDB.dataThreshold", threshold);
}

function resetDataThreshold() {
  info("Clearing data threshold pref");
  SpecialPowers.clearUserPref("dom.indexedDB.dataThreshold");
}

function setMaxSerializedMsgSize(aSize) {
  info("Setting maximal size of a serialized message to " + aSize);
  SpecialPowers.setIntPref("dom.indexedDB.maxSerializedMsgSize", aSize);
}

function enablePreprocessing() {
  info("Setting preprocessing pref");
  SpecialPowers.setBoolPref("dom.indexedDB.preprocessing", true);
}

function resetPreprocessing() {
  info("Clearing preprocessing pref");
  SpecialPowers.clearUserPref("dom.indexedDB.preprocessing");
}

function getPrincipal(url) {
  let uri = Services.io.newURI(url);
  return Services.scriptSecurityManager.createContentPrincipal(uri, {});
}

function requestFinished(request) {
  return new Promise(function(resolve, reject) {
    request.callback = function(req) {
      if (req.resultCode == Cr.NS_OK) {
        resolve(req.result);
      } else {
        reject(req.resultCode);
      }
    };
  });
}

// TODO: Rename to openDBRequestSucceeded ?
function expectingSuccess(request) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onsuccess = function(event) {
      resolve(event);
    };
    request.onupgradeneeded = function(event) {
      ok(false, "Got upgrade, but did not expect it!");
      reject(event);
    };
  });
}

// TODO: Rename to openDBRequestUpgradeNeeded ?
function expectingUpgrade(request) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onupgradeneeded = function(event) {
      resolve(event);
    };
    request.onsuccess = function(event) {
      ok(false, "Got success, but did not expect it!");
      reject(event);
    };
  });
}

function requestSucceeded(request) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onsuccess = function(event) {
      resolve(event);
    };
  });
}

// Given a "/"-delimited path relative to the profile directory,
// return an nsIFile representing the path.  This does not test
// for the existence of the file or parent directories.
// It is safe even on Windows where the directory separator is not "/",
// but make sure you're not passing in a "\"-delimited path.
function getRelativeFile(relativePath) {
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

  let file = profileDir.clone();
  relativePath.split("/").forEach(function(component) {
    file.append(component);
  });

  return file;
}

var SpecialPowers = {
  isMainProcess() {
    return (
      Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );
  },
  notifyObservers(subject, topic, data) {
    Services.obs.notifyObservers(subject, topic, data);
  },
  notifyObserversInParentProcess(subject, topic, data) {
    if (subject) {
      throw new Error("Can't send subject to another process!");
    }
    return this.notifyObservers(subject, topic, data);
  },
  getBoolPref(prefName) {
    return Services.prefs.getBoolPref(prefName);
  },
  setBoolPref(prefName, value) {
    Services.prefs.setBoolPref(prefName, value);
  },
  setIntPref(prefName, value) {
    Services.prefs.setIntPref(prefName, value);
  },
  clearUserPref(prefName) {
    Services.prefs.clearUserPref(prefName);
  },
  // Copied (and slightly adjusted) from testing/specialpowers/content/SpecialPowersAPI.jsm
  exactGC(callback) {
    let count = 0;

    function doPreciseGCandCC() {
      function scheduledGCCallback() {
        Cu.forceCC();

        if (++count < 3) {
          doPreciseGCandCC();
        } else {
          callback();
        }
      }

      Cu.schedulePreciseGC(scheduledGCCallback);
    }

    doPreciseGCandCC();
  },

  get Cc() {
    return Cc;
  },

  get Ci() {
    return Ci;
  },

  get Cu() {
    return Cu;
  },

  // Based on SpecialPowersObserver.prototype.receiveMessage
  createFiles(requests, callback) {
    let filePaths = [];
    if (!this._createdFiles) {
      this._createdFiles = [];
    }
    let createdFiles = this._createdFiles;
    let promises = [];
    requests.forEach(function(request) {
      const filePerms = 0o666;
      let testFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
      if (request.name) {
        testFile.append(request.name);
      } else {
        testFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, filePerms);
      }
      let outStream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      outStream.init(
        testFile,
        0x02 | 0x08 | 0x20, // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
        filePerms,
        0
      );
      if (request.data) {
        outStream.write(request.data, request.data.length);
        outStream.close();
      }
      promises.push(
        File.createFromFileName(testFile.path, request.options).then(function(
          file
        ) {
          filePaths.push(file);
        })
      );
      createdFiles.push(testFile);
    });

    Promise.all(promises).then(function() {
      setTimeout(function() {
        callback(filePaths);
      }, 0);
    });
  },

  removeFiles() {
    if (this._createdFiles) {
      this._createdFiles.forEach(function(testFile) {
        try {
          testFile.remove(false);
        } catch (e) {}
      });
      this._createdFiles = null;
    }
  },
};
