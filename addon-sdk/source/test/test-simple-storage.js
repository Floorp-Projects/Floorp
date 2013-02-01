/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const file = require("sdk/io/file");
const prefs = require("sdk/preferences/service");

const QUOTA_PREF = "extensions.addon-sdk.simple-storage.quota";

let {Cc,Ci} = require("chrome");

const { Loader } = require("sdk/test/loader");
const { id } = require("sdk/self");

let storeFile = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties).
                get("ProfD", Ci.nsIFile);
storeFile.append("jetpack");
storeFile.append(id);
storeFile.append("simple-storage");
storeFile.append("store.json");
let storeFilename = storeFile.path;

function manager(loader) loader.sandbox("sdk/simple-storage").manager;

exports.testSetGet = function (test) {
  test.waitUntilDone();

  // Load the module once, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    test.assert(file.exists(storeFilename), "Store file should exist");

    // Load the module again and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    manager(loader).jsonStore.onWrite = function (storage) {
      file.remove(storeFilename);
      test.done();
    };
    test.assertEqual(ss.storage.foo, val, "Value should persist");
    loader.unload();
  };
  let val = "foo";
  ss.storage.foo = val;
  test.assertEqual(ss.storage.foo, val, "Value read should be value set");
  loader.unload();
};

exports.testSetGetRootArray = function (test) {
  setGetRoot(test, [1, 2, 3], function (arr1, arr2) {
    if (arr1.length !== arr2.length)
      return false;
    for (let i = 0; i < arr1.length; i++) {
      if (arr1[i] !== arr2[i])
        return false;
    }
    return true;
  });
};

exports.testSetGetRootBool = function (test) {
  setGetRoot(test, true);
};

exports.testSetGetRootFunction = function (test) {
  setGetRootError(test, function () {},
                  "Setting storage to a function should fail");
};

exports.testSetGetRootNull = function (test) {
  setGetRoot(test, null);
};

exports.testSetGetRootNumber = function (test) {
  setGetRoot(test, 3.14);
};

exports.testSetGetRootObject = function (test) {
  setGetRoot(test, { foo: 1, bar: 2 }, function (obj1, obj2) {
    for (let prop in obj1) {
      if (!(prop in obj2) || obj2[prop] !== obj1[prop])
        return false;
    }
    for (let prop in obj2) {
      if (!(prop in obj1) || obj1[prop] !== obj2[prop])
        return false;
    }
    return true;
  });
};

exports.testSetGetRootString = function (test) {
  setGetRoot(test, "sho' 'nuff");
};

exports.testSetGetRootUndefined = function (test) {
  setGetRootError(test, undefined, "Setting storage to undefined should fail");
};

exports.testEmpty = function (test) {
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  loader.unload();
  test.assert(!file.exists(storeFilename), "Store file should not exist");
};

exports.testMalformed = function (test) {
  let stream = file.open(storeFilename, "w");
  stream.write("i'm not json");
  stream.close();
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  let empty = true;
  for (let key in ss.storage) {
    empty = false;
    break;
  }
  test.assert(empty, "Malformed storage should cause root to be empty");
  loader.unload();
};

// Go over quota and handle it by listener.
exports.testQuotaExceededHandle = function (test) {
  test.waitUntilDone();
  prefs.set(QUOTA_PREF, 18);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  ss.on("OverQuota", function () {
    test.pass("OverQuota was emitted as expected");
    test.assertEqual(this, ss, "`this` should be simple storage");
    ss.storage = { x: 4, y: 5 };

    manager(loader).jsonStore.onWrite = function () {
      loader = Loader(module);
      ss = loader.require("sdk/simple-storage");
      let numProps = 0;
      for (let prop in ss.storage)
        numProps++;
      test.assert(numProps, 2,
                  "Store should contain 2 values: " + ss.storage.toSource());
      test.assertEqual(ss.storage.x, 4, "x value should be correct");
      test.assertEqual(ss.storage.y, 5, "y value should be correct");
      manager(loader).jsonStore.onWrite = function (storage) {
        prefs.reset(QUOTA_PREF);
        test.done();
      };
      loader.unload();
    };
    loader.unload();
  });
  // This will be JSON.stringify()ed to: {"a":1,"b":2,"c":3} (19 bytes)
  ss.storage = { a: 1, b: 2, c: 3 };
  manager(loader).jsonStore.write();
};

// Go over quota but don't handle it.  The last good state should still persist.
exports.testQuotaExceededNoHandle = function (test) {
  test.waitUntilDone();
  prefs.set(QUOTA_PREF, 5);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");

  manager(loader).jsonStore.onWrite = function (storage) {
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    test.assertEqual(ss.storage, val,
                     "Value should have persisted: " + ss.storage);
    ss.storage = "some very long string that is very long";
    ss.on("OverQuota", function () {
      test.pass("OverQuota emitted as expected");
      manager(loader).jsonStore.onWrite = function () {
        test.fail("Over-quota value should not have been written");
      };
      loader.unload();

      loader = Loader(module);
      ss = loader.require("sdk/simple-storage");
      test.assertEqual(ss.storage, val,
                       "Over-quota value should not have been written, " +
                       "old value should have persisted: " + ss.storage);
      loader.unload();
      prefs.reset(QUOTA_PREF);
      test.done();
    });
    manager(loader).jsonStore.write();
  };

  let val = "foo";
  ss.storage = val;
  loader.unload();
};

exports.testQuotaUsage = function (test) {
  test.waitUntilDone();

  let quota = 21;
  prefs.set(QUOTA_PREF, quota);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");

  // {"a":1} (7 bytes)
  ss.storage = { a: 1 };
  test.assertEqual(ss.quotaUsage, 7 / quota, "quotaUsage should be correct");

  // {"a":1,"bb":2} (14 bytes)
  ss.storage = { a: 1, bb: 2 };
  test.assertEqual(ss.quotaUsage, 14 / quota, "quotaUsage should be correct");

  // {"a":1,"bb":2,"cc":3} (21 bytes)
  ss.storage = { a: 1, bb: 2, cc: 3 };
  test.assertEqual(ss.quotaUsage, 21 / quota, "quotaUsage should be correct");

  manager(loader).jsonStore.onWrite = function () {
    prefs.reset(QUOTA_PREF);
    test.done();
  };
  loader.unload();
};

exports.testUninstall = function (test) {
  test.waitUntilDone();
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function () {
    test.assert(file.exists(storeFilename), "Store file should exist");

    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    loader.unload("uninstall");
    test.assert(!file.exists(storeFilename), "Store file should be removed");
    test.done();
  };
  ss.storage.foo = "foo";
  loader.unload();
};

exports.testSetNoSetRead = function (test) {
  test.waitUntilDone();

  // Load the module, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    test.assert(file.exists(storeFilename), "Store file should exist");

    // Load the module again but don't access ss.storage.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    manager(loader).jsonStore.onWrite = function (storage) {
      test.fail("Nothing should be written since `storage` was not accessed.");
    };
    loader.unload();

    // Load the module a third time and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    manager(loader).jsonStore.onWrite = function (storage) {
      file.remove(storeFilename);
      test.done();
    };
    test.assertEqual(ss.storage.foo, val, "Value should persist");
    loader.unload();
  };
  let val = "foo";
  ss.storage.foo = val;
  test.assertEqual(ss.storage.foo, val, "Value read should be value set");
  loader.unload();
};


function setGetRoot(test, val, compare) {
  test.waitUntilDone();

  compare = compare || function (a, b) a === b;

  // Load the module once, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function () {
    test.assert(file.exists(storeFilename), "Store file should exist");

    // Load the module again and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    manager(loader).jsonStore.onWrite = function () {
      file.remove(storeFilename);
      test.done();
    };
    test.assert(compare(ss.storage, val), "Value should persist");
    loader.unload();
  };
  ss.storage = val;
  test.assert(compare(ss.storage, val), "Value read should be value set");
  loader.unload();
}

function setGetRootError(test, val, msg) {
  let pred = "storage must be one of the following types: " +
             "array, boolean, null, number, object, string";
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  test.assertRaises(function () ss.storage = val, pred, msg);
  loader.unload();
}
