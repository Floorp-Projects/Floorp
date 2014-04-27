/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const file = require("sdk/io/file");
const prefs = require("sdk/preferences/service");

const QUOTA_PREF = "extensions.addon-sdk.simple-storage.quota";
const WRITE_PERIOD_PREF = "extensions.addon-sdk.simple-storage.writePeriod";

let {Cc,Ci} = require("chrome");

const { Loader } = require("sdk/test/loader");
const { id } = require("sdk/self");

let storeFile = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties).
                get("ProfD", Ci.nsIFile);
storeFile.append("jetpack");
storeFile.append(id);
storeFile.append("simple-storage");
file.mkpath(storeFile.path);
storeFile.append("store.json");
let storeFilename = storeFile.path;

function manager(loader) loader.sandbox("sdk/simple-storage").manager;

exports.testSetGet = function (assert, done) {
  // Load the module once, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    // Load the module again and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.equal(ss.storage.foo, val, "Value should persist");
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.fail("Nothing should be written since `storage` was not changed.");
    };
    loader.unload();
    file.remove(storeFilename);
    done();
  };
  let val = "foo";
  ss.storage.foo = val;
  assert.equal(ss.storage.foo, val, "Value read should be value set");
  loader.unload();
};

exports.testSetGetRootArray = function (assert, done) {
  setGetRoot(assert, done, [1, 2, 3], function (arr1, arr2) {
    if (arr1.length !== arr2.length)
      return false;
    for (let i = 0; i < arr1.length; i++) {
      if (arr1[i] !== arr2[i])
        return false;
    }
    return true;
  });
};

exports.testSetGetRootBool = function (assert, done) {
  setGetRoot(assert, done, true);
};

exports.testSetGetRootFunction = function (assert, done) {
  setGetRootError(assert, done, function () {},
                  "Setting storage to a function should fail");
};

exports.testSetGetRootNull = function (assert, done) {
  setGetRoot(assert, done, null);
};

exports.testSetGetRootNumber = function (assert, done) {
  setGetRoot(assert, done, 3.14);
};

exports.testSetGetRootObject = function (assert, done) {
  setGetRoot(assert, done, { foo: 1, bar: 2 }, function (obj1, obj2) {
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

exports.testSetGetRootString = function (assert, done) {
  setGetRoot(assert, done, "sho' 'nuff");
};

exports.testSetGetRootUndefined = function (assert, done) {
  setGetRootError(assert, done, undefined, "Setting storage to undefined should fail");
};

exports.testEmpty = function (assert) {
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  loader.unload();
  assert.ok(!file.exists(storeFilename), "Store file should not exist");
};

exports.testStorageDataRecovery = function(assert) {
  const data = { 
    a: true,
    b: [3, 13],
    c: "guilty!",
    d: { e: 1, f: 2 }
  };
  let stream = file.open(storeFilename, "w");
  stream.write(JSON.stringify(data));
  stream.close();
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  assert.deepEqual(ss.storage, data, "Recovered data should be the same as written");
  file.remove(storeFilename);
  loader.unload();
}

exports.testMalformed = function (assert) {
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
  assert.ok(empty, "Malformed storage should cause root to be empty");
  file.remove(storeFilename);
  loader.unload();
};

// Go over quota and handle it by listener.
exports.testQuotaExceededHandle = function (assert, done) {
  prefs.set(QUOTA_PREF, 18);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  ss.on("OverQuota", function () {
    assert.pass("OverQuota was emitted as expected");
    assert.equal(this, ss, "`this` should be simple storage");
    ss.storage = { x: 4, y: 5 };

    manager(loader).jsonStore.onWrite = function () {
      loader = Loader(module);
      ss = loader.require("sdk/simple-storage");
      let numProps = 0;
      for (let prop in ss.storage)
        numProps++;
      assert.ok(numProps, 2,
                  "Store should contain 2 values: " + ss.storage.toSource());
      assert.equal(ss.storage.x, 4, "x value should be correct");
      assert.equal(ss.storage.y, 5, "y value should be correct");
      manager(loader).jsonStore.onWrite = function (storage) {
        assert.fail("Nothing should be written since `storage` was not changed.");
      };
      loader.unload();
      prefs.reset(QUOTA_PREF);
      done();
    };
    loader.unload();
  });
  // This will be JSON.stringify()ed to: {"a":1,"b":2,"c":3} (19 bytes)
  ss.storage = { a: 1, b: 2, c: 3 };
  manager(loader).jsonStore.write();
};

// Go over quota but don't handle it.  The last good state should still persist.
exports.testQuotaExceededNoHandle = function (assert, done) {
  prefs.set(QUOTA_PREF, 5);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");

  manager(loader).jsonStore.onWrite = function (storage) {
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.equal(ss.storage, val,
                     "Value should have persisted: " + ss.storage);
    ss.storage = "some very long string that is very long";
    ss.on("OverQuota", function () {
      assert.pass("OverQuota emitted as expected");
      manager(loader).jsonStore.onWrite = function () {
        assert.fail("Over-quota value should not have been written");
      };
      loader.unload();

      loader = Loader(module);
      ss = loader.require("sdk/simple-storage");
      assert.equal(ss.storage, val,
                       "Over-quota value should not have been written, " +
                       "old value should have persisted: " + ss.storage);
      manager(loader).jsonStore.onWrite = function (storage) {
        assert.fail("Nothing should be written since `storage` was not changed.");
      };
      loader.unload();
      prefs.reset(QUOTA_PREF);
      done();
    });
    manager(loader).jsonStore.write();
  };

  let val = "foo";
  ss.storage = val;
  loader.unload();
};

exports.testQuotaUsage = function (assert, done) {
  let quota = 21;
  prefs.set(QUOTA_PREF, quota);

  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");

  // {"a":1} (7 bytes)
  ss.storage = { a: 1 };
  assert.equal(ss.quotaUsage, 7 / quota, "quotaUsage should be correct");

  // {"a":1,"bb":2} (14 bytes)
  ss.storage = { a: 1, bb: 2 };
  assert.equal(ss.quotaUsage, 14 / quota, "quotaUsage should be correct");

  // {"a":1,"bb":2,"cc":3} (21 bytes)
  ss.storage = { a: 1, bb: 2, cc: 3 };
  assert.equal(ss.quotaUsage, 21 / quota, "quotaUsage should be correct");

  manager(loader).jsonStore.onWrite = function () {
    prefs.reset(QUOTA_PREF);
    done();
  };
  loader.unload();
};

exports.testUninstall = function (assert, done) {
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function () {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    loader.unload("uninstall");
    assert.ok(!file.exists(storeFilename), "Store file should be removed");
    done();
  };
  ss.storage.foo = "foo";
  loader.unload();
};

exports.testChangeInnerArray = function(assert, done) {
  prefs.set(WRITE_PERIOD_PREF, 10);

  let expected = {
    x: [5, 7],
    y: [7, 28],
    z: [6, 2]
  };

  // Load the module, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    // Load the module again and check the result
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.equal(JSON.stringify(ss.storage),
                     JSON.stringify(expected), "Should see the expected object");

    // Add a property
    ss.storage.x.push(["bar"]);
    expected.x.push(["bar"]);
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.equal(JSON.stringify(ss.storage),
                       JSON.stringify(expected), "Should see the expected object");

      // Modify a property
      ss.storage.y[0] = 42;
      expected.y[0] = 42;
      manager(loader).jsonStore.onWrite = function (storage) {
        assert.equal(JSON.stringify(ss.storage),
                         JSON.stringify(expected), "Should see the expected object");

        // Delete a property
        delete ss.storage.z[1];
        delete expected.z[1];
        manager(loader).jsonStore.onWrite = function (storage) {
          assert.equal(JSON.stringify(ss.storage),
                           JSON.stringify(expected), "Should see the expected object");

          // Modify the new inner-object
          ss.storage.x[2][0] = "baz";
          expected.x[2][0] = "baz";
          manager(loader).jsonStore.onWrite = function (storage) {
            assert.equal(JSON.stringify(ss.storage),
                             JSON.stringify(expected), "Should see the expected object");

            manager(loader).jsonStore.onWrite = function (storage) {
              assert.fail("Nothing should be written since `storage` was not changed.");
            };
            loader.unload();

            // Load the module again and check the result
            loader = Loader(module);
            ss = loader.require("sdk/simple-storage");
            assert.equal(JSON.stringify(ss.storage),
                             JSON.stringify(expected), "Should see the expected object");
            loader.unload();
            file.remove(storeFilename);
            prefs.reset(WRITE_PERIOD_PREF);
            done();
          };
        };
      };
    };
  };

  ss.storage = {
    x: [5, 7],
    y: [7, 28],
    z: [6, 2]
  };
  assert.equal(JSON.stringify(ss.storage),
                   JSON.stringify(expected), "Should see the expected object");

  loader.unload();
};

exports.testChangeInnerObject = function(assert, done) {
  prefs.set(WRITE_PERIOD_PREF, 10);

  let expected = {
    x: {
      a: 5,
      b: 7
    },
    y: {
      c: 7,
      d: 28
    },
    z: {
      e: 6,
      f: 2
    }
  };

  // Load the module, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    // Load the module again and check the result
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.equal(JSON.stringify(ss.storage),
                     JSON.stringify(expected), "Should see the expected object");

    // Add a property
    ss.storage.x.g = {foo: "bar"};
    expected.x.g = {foo: "bar"};
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.equal(JSON.stringify(ss.storage),
                       JSON.stringify(expected), "Should see the expected object");

      // Modify a property
      ss.storage.y.c = 42;
      expected.y.c = 42;
      manager(loader).jsonStore.onWrite = function (storage) {
        assert.equal(JSON.stringify(ss.storage),
                         JSON.stringify(expected), "Should see the expected object");

        // Delete a property
        delete ss.storage.z.f;
        delete expected.z.f;
        manager(loader).jsonStore.onWrite = function (storage) {
          assert.equal(JSON.stringify(ss.storage),
                           JSON.stringify(expected), "Should see the expected object");

          // Modify the new inner-object
          ss.storage.x.g.foo = "baz";
          expected.x.g.foo = "baz";
          manager(loader).jsonStore.onWrite = function (storage) {
            assert.equal(JSON.stringify(ss.storage),
                             JSON.stringify(expected), "Should see the expected object");

            manager(loader).jsonStore.onWrite = function (storage) {
              assert.fail("Nothing should be written since `storage` was not changed.");
            };
            loader.unload();

            // Load the module again and check the result
            loader = Loader(module);
            ss = loader.require("sdk/simple-storage");
            assert.equal(JSON.stringify(ss.storage),
                             JSON.stringify(expected), "Should see the expected object");
            loader.unload();
            file.remove(storeFilename);
            prefs.reset(WRITE_PERIOD_PREF);
            done();
          };
        };
      };
    };
  };

  ss.storage = {
    x: {
      a: 5,
      b: 7
    },
    y: {
      c: 7,
      d: 28
    },
    z: {
      e: 6,
      f: 2
    }
  };
  assert.equal(JSON.stringify(ss.storage),
                   JSON.stringify(expected), "Should see the expected object");

  loader.unload();
};

exports.testSetNoSetRead = function (assert, done) {
  // Load the module, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function (storage) {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    // Load the module again but don't access ss.storage.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.fail("Nothing should be written since `storage` was not accessed.");
    };
    loader.unload();

    // Load the module a third time and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.equal(ss.storage.foo, val, "Value should persist");
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.fail("Nothing should be written since `storage` was not changed.");
    };
    loader.unload();
    file.remove(storeFilename);
    done();
  };
  let val = "foo";
  ss.storage.foo = val;
  assert.equal(ss.storage.foo, val, "Value read should be value set");
  loader.unload();
};


function setGetRoot(assert, done, val, compare) {
  compare = compare || function (a, b) a === b;

  // Load the module once, set a value.
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  manager(loader).jsonStore.onWrite = function () {
    assert.ok(file.exists(storeFilename), "Store file should exist");

    // Load the module again and make sure the value stuck.
    loader = Loader(module);
    ss = loader.require("sdk/simple-storage");
    assert.ok(compare(ss.storage, val), "Value should persist");
    manager(loader).jsonStore.onWrite = function (storage) {
      assert.fail("Nothing should be written since `storage` was not changed.");
    };
    loader.unload();
    file.remove(storeFilename);
    done();
  };
  ss.storage = val;
  assert.ok(compare(ss.storage, val), "Value read should be value set");
  loader.unload();
}

function setGetRootError(assert, done, val, msg) {
  let pred = new RegExp("storage must be one of the following types: " +
             "array, boolean, null, number, object, string");
  let loader = Loader(module);
  let ss = loader.require("sdk/simple-storage");
  assert.throws(function () ss.storage = val, pred, msg);
  done();
  loader.unload();
}

require('sdk/test').run(exports);
