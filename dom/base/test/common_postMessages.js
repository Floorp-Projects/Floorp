function setup_tests() {
  SpecialPowers.pushPrefEnv({ set: [["dom.input.dirpicker", true]] }, next);
}

function getType(a) {
  if (a === null || a === undefined) {
    return "null";
  }

  if (Array.isArray(a)) {
    return "array";
  }

  if (typeof a == "object") {
    return "object";
  }

  if (
    SpecialPowers.Cu.getJSTestingFunctions().wasmIsSupported() &&
    a instanceof WebAssembly.Module
  ) {
    return "wasm";
  }

  return "primitive";
}

function compare(a, b) {
  is(getType(a), getType(b), "Type matches");

  var type = getType(a);
  if (type == "array") {
    is(a.length, b.length, "Array.length matches");
    for (var i = 0; i < a.length; ++i) {
      compare(a[i], b[i]);
    }

    return;
  }

  if (type == "object") {
    ok(a !== b, "They should not match");

    var aProps = [];
    for (var p in a) {
      aProps.push(p);
    }

    var bProps = [];
    for (var p in b) {
      bProps.push(p);
    }

    is(aProps.length, bProps.length, "Props match");
    is(aProps.sort().toString(), bProps.sort().toString(), "Props names match");

    for (var p in a) {
      compare(a[p], b[p]);
    }

    return;
  }

  if (type == "wasm") {
    var wasmA = new WebAssembly.Instance(a);
    ok(wasmA instanceof WebAssembly.Instance, "got an instance");

    var wasmB = new WebAssembly.Instance(b);
    ok(wasmB instanceof WebAssembly.Instance, "got an instance");

    ok(wasmA.exports.foo() === wasmB.exports.foo(), "Same result!");
    ok(wasmB.exports.foo() === 42, "We want 42");
  }

  if (type != "null") {
    is(a, b, "Same value");
  }
}

var clonableObjects = [
  { target: "all", data: "hello world" },
  { target: "all", data: 123 },
  { target: "all", data: null },
  { target: "all", data: true },
  { target: "all", data: new Date() },
  { target: "all", data: [1, "test", true, new Date()] },
  {
    target: "all",
    data: { a: true, b: null, c: new Date(), d: [true, false, {}] },
  },
  { target: "all", data: new Blob([123], { type: "plain/text" }) },
  { target: "all", data: new ImageData(2, 2) },
];

function create_fileList() {
  var url = SimpleTest.getTestFileURL("script_postmessages_fileList.js");
  var script = SpecialPowers.loadChromeScript(url);

  function onOpened(message) {
    var fileList = document.getElementById("fileList");
    SpecialPowers.wrap(fileList).mozSetFileArray([message.file]);

    // Just a simple test
    var domFile = fileList.files[0];
    is(domFile.name, "prefs.js", "fileName should be prefs.js");

    clonableObjects.push({ target: "all", data: fileList.files });
    script.destroy();
    next();
  }

  script.addMessageListener("file.opened", onOpened);
  script.sendAsyncMessage("file.open");
}

function create_directory() {
  if (navigator.userAgent.toLowerCase().includes("Android")) {
    next();
    return;
  }

  var url = SimpleTest.getTestFileURL("script_postmessages_fileList.js");
  var script = SpecialPowers.loadChromeScript(url);

  function onOpened(message) {
    var fileList = document.getElementById("fileList");
    SpecialPowers.wrap(fileList).mozSetDirectory(message.dir);

    fileList.getFilesAndDirectories().then(function(list) {
      // Just a simple test
      is(list.length, 1, "This list has 1 element");
      ok(list[0] instanceof Directory, "We have a directory.");

      clonableObjects.push({ target: "all", data: list[0] });
      script.destroy();
      next();
    });
  }

  script.addMessageListener("dir.opened", onOpened);
  script.sendAsyncMessage("dir.open");
}

function create_wasmModule() {
  info("Checking if we can play with WebAssembly...");

  if (!SpecialPowers.Cu.getJSTestingFunctions().wasmIsSupported()) {
    next();
    return;
  }

  ok(WebAssembly, "WebAssembly object should exist");
  ok(WebAssembly.compile, "WebAssembly.compile function should exist");

  /*
    js -e '
      t = wasmTextToBinary(`
        (module
          (func $foo (result i32) (i32.const 42))
          (export "foo" (func $foo))
        )
      `);
      print(t)
    '
  */
  // eslint-disable-next-line
  const fooModuleCode = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,3,2,1,0,7,7,1,3,102,111,111,0,0,10,6,1,4,0,65,42,11,0,13,4,110,97,109,101,1,6,1,0,3,102,111,111]);

  WebAssembly.compile(fooModuleCode).then(
    m => {
      ok(m instanceof WebAssembly.Module, "The WasmModule has been compiled.");
      clonableObjects.push({ target: "sameProcess", data: m });
      next();
    },
    () => {
      ok(false, "The compilation of the wasmModule failed.");
    }
  );
}

function runTests(obj) {
  ok(
    "clonableObjectsEveryWhere" in obj &&
      "clonableObjectsSameProcess" in obj &&
      "transferableObjects" in obj &&
      (obj.clonableObjectsEveryWhere ||
        obj.clonableObjectsSameProcess ||
        obj.transferableObjects),
    "We must run some test!"
  );

  // cloning tests - everyWhere
  new Promise(function(resolve, reject) {
    var clonableObjectsId = 0;
    function runClonableTest() {
      if (clonableObjectsId >= clonableObjects.length) {
        resolve();
        return;
      }

      var object = clonableObjects[clonableObjectsId++];

      if (object.target != "all") {
        runClonableTest();
        return;
      }

      obj
        .send(object.data, [])
        .catch(() => {
          return { error: true };
        })
        .then(received => {
          if (!obj.clonableObjectsEveryWhere) {
            ok(received.error, "Error expected");
          } else {
            ok(!received.error, "Error not expected");
            compare(received.data, object.data);
          }
          runClonableTest();
        });
    }

    runClonableTest();
  })

    // clonable same process
    .then(function() {
      return new Promise(function(resolve, reject) {
        var clonableObjectsId = 0;
        function runClonableTest() {
          if (clonableObjectsId >= clonableObjects.length) {
            resolve();
            return;
          }

          var object = clonableObjects[clonableObjectsId++];

          if (object.target != "sameProcess") {
            runClonableTest();
            return;
          }

          obj
            .send(object.data, [])
            .catch(() => {
              return { error: true };
            })
            .then(received => {
              if (!obj.clonableObjectsSameProcess) {
                ok(received.error, "Error expected");
              } else {
                ok(!received.error, "Error not expected");
                compare(received.data, object.data);
              }
              runClonableTest();
            });
        }

        runClonableTest();
      });
    })

    // transfering tests
    .then(function() {
      if (!obj.transferableObjects) {
        return;
      }

      // MessagePort
      return new Promise(function(r, rr) {
        var mc = new MessageChannel();
        obj.send(42, [mc.port1]).then(function(received) {
          is(received.ports.length, 1, "MessagePort has been transferred");
          mc.port2.postMessage("hello world");
          received.ports[0].onmessage = function(e) {
            is(e.data, "hello world", "Ports are connected!");
            r();
          };
        });
      });
    })

    // no dup transfering
    .then(function() {
      if (!obj.transferableObjects) {
        return;
      }

      // MessagePort
      return new Promise(function(r, rr) {
        var mc = new MessageChannel();
        obj
          .send(42, [mc.port1, mc.port1])
          .then(
            function(received) {
              ok(false, "Duplicate ports should throw!");
            },
            function() {
              ok(true, "Duplicate ports should throw!");
            }
          )
          .then(r);
      });
    })

    // non transfering tests
    .then(function() {
      if (obj.transferableObjects) {
        return;
      }

      // MessagePort
      return new Promise(function(r, rr) {
        var mc = new MessageChannel();
        obj
          .send(42, [mc.port1])
          .then(
            function(received) {
              ok(false, "This object should not support port transferring");
            },
            function() {
              ok(true, "This object should not support port transferring");
            }
          )
          .then(r);
      });
    })

    // done.
    .then(function() {
      obj.finished();
    });
}

function next() {
  if (!tests.length) {
    SimpleTest.finish();
    return;
  }

  var test = tests.shift();
  test();
}

var tests = [setup_tests, create_fileList, create_directory, create_wasmModule];
