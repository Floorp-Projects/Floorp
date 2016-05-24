// This is a list of all interfaces that are exposed to workers.
// Please only add things to this list with great care and proper review
// from the associated module peers.

// This file lists global interfaces we want exposed and verifies they
// are what we intend. Each entry in the arrays below can either be a
// simple string with the interface name, or an object with a 'name'
// property giving the interface name as a string, and additional
// properties which qualify the exposure of that interface. For example:
//
// [
//   "AGlobalInterface",
//   { name: "ExperimentalThing", release: false },
//   { name: "ReallyExperimentalThing", nightly: true },
//   { name: "DesktopOnlyThing", desktop: true },
//   { name: "NonB2gOnlyThing", b2g: false },
//   { name: "FancyControl", xbl: true },
//   { name: "DisabledEverywhere", disabled: true },
// ];
//
// See createInterfaceMap() below for a complete list of properties.

// IMPORTANT: Do not change this list without review from
//            a JavaScript Engine peer!
var ecmaGlobals =
  [
    "Array",
    "ArrayBuffer",
    "Boolean",
    "DataView",
    "Date",
    "Error",
    "EvalError",
    "Float32Array",
    "Float64Array",
    "Function",
    "Infinity",
    "Int16Array",
    "Int32Array",
    "Int8Array",
    "InternalError",
    {name: "Intl", android: false},
    "Iterator",
    "JSON",
    "Map",
    "Math",
    "NaN",
    "Number",
    "Object",
    "Proxy",
    "RangeError",
    "ReferenceError",
    "Reflect",
    "RegExp",
    "Set",
    {name: "SharedArrayBuffer", nightly: true},
    {name: "SIMD", nightly: true},
    {name: "Atomics", nightly: true},
    "StopIteration",
    "String",
    "Symbol",
    "SyntaxError",
    {name: "TypedObject", nightly: true},
    "TypeError",
    "Uint16Array",
    "Uint32Array",
    "Uint8Array",
    "Uint8ClampedArray",
    "URIError",
    "WeakMap",
    "WeakSet",
  ];
// IMPORTANT: Do not change the list above without review from
//            a JavaScript Engine peer!

// IMPORTANT: Do not change the list below without review from a DOM peer!
var interfaceNamesInGlobalScope =
  [
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Blob",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "BroadcastChannel",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Cache",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "CacheStorage",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Client",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Clients",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Crypto",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "CustomEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Directory",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "DOMCursor",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "DOMError",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "DOMException",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "DOMRequest",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "DOMStringList",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Event",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "EventTarget",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ExtendableEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ExtendableMessageEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "FetchEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "File",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "FileReader",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "FileReaderSync",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "FormData",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Headers",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBCursor",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBCursorWithValue",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBDatabase",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBFactory",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBIndex",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBKeyRange",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBObjectStore",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBOpenDBRequest",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBRequest",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBTransaction",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "IDBVersionChangeEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ImageBitmap",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ImageBitmapRenderingContext",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ImageData",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "MessageChannel",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "MessageEvent",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "MessagePort",
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "Notification", nonReleaseB2G: true, nonReleaseAndroid: true,
                            b2g: false, android: false },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "NotificationEvent", nonReleaseB2G: true, nonReleaseAndroid: true,
                                 b2g: false, android: false },
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Performance",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "PerformanceEntry",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "PerformanceMark",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "PerformanceMeasure",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Promise",
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "PushEvent", b2g: false, android: false, nightlyAndroid: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "PushManager", b2g: false, android: false, nightlyAndroid: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "PushMessageData", b2g: false, android: false, nightlyAndroid: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "PushSubscription", b2g: false, android: false, nightlyAndroid: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "PushSubscriptionOptions", b2g: false, android: false, nightlyAndroid: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Request",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "Response",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ServiceWorker",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ServiceWorkerGlobalScope",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "ServiceWorkerRegistration",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "SubtleCrypto",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "TextDecoder",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "TextEncoder",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "URL",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "URLSearchParams",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "WebSocket",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "WindowClient",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "WorkerGlobalScope",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "WorkerLocation",
// IMPORTANT: Do not change this list without review from a DOM peer!
    "WorkerNavigator",
// IMPORTANT: Do not change this list without review from a DOM peer!
  ];
// IMPORTANT: Do not change the list above without review from a DOM peer!

function createInterfaceMap(permissionMap, version, userAgent, isB2G) {
  var isNightly = version.endsWith("a1");
  var isRelease = !version.includes("a");
  var isDesktop = !/Mobile|Tablet/.test(userAgent);
  var isAndroid = !!navigator.userAgent.includes("Android");

  var interfaceMap = {};

  function addInterfaces(interfaces)
  {
    for (var entry of interfaces) {
      if (typeof(entry) === "string") {
        interfaceMap[entry] = true;
      } else {
        ok(!("pref" in entry), "Bogus pref annotation for " + entry.name);
        if ((entry.nightly === !isNightly) ||
            (entry.nightlyAndroid === !(isAndroid && isNightly) && isAndroid) ||
            (entry.nonReleaseB2G === !(isB2G && !isRelease) && isB2G) ||
            (entry.nonReleaseAndroid === !(isAndroid && !isRelease) && isAndroid) ||
            (entry.desktop === !isDesktop) ||
            (entry.android === !isAndroid && !entry.nonReleaseAndroid && !entry.nightlyAndroid) ||
            (entry.b2g === !isB2G && !entry.nonReleaseB2G) ||
            (entry.release === !isRelease) ||
            (entry.permission && !permissionMap[entry.permission])) {
          interfaceMap[entry.name] = false;
        } else {
          interfaceMap[entry.name] = true;
        }
      }
    }
  }

  addInterfaces(ecmaGlobals);
  addInterfaces(interfaceNamesInGlobalScope);

  return interfaceMap;
}

function runTest(permissionMap, version, userAgent, isB2G) {
  var interfaceMap = createInterfaceMap(permissionMap, version, userAgent, isB2G);
  for (var name of Object.getOwnPropertyNames(self)) {
    // An interface name should start with an upper case character.
    if (!/^[A-Z]/.test(name)) {
      continue;
    }
    ok(interfaceMap[name],
       "If this is failing: DANGER, are you sure you want to expose the new interface " + name +
       " to all webpages as a property on the service worker? Do not make a change to this file without a " +
       " review from a DOM peer for that specific change!!! (or a JS peer for changes to ecmaGlobals)");
    delete interfaceMap[name];
  }
  for (var name of Object.keys(interfaceMap)) {
    ok(name in self === interfaceMap[name],
       name + " should " + (interfaceMap[name] ? "" : " NOT") + " be defined on the global scope");
    if (!interfaceMap[name]) {
      delete interfaceMap[name];
    }
  }
  is(Object.keys(interfaceMap).length, 0,
     "The following interface(s) are not enumerated: " + Object.keys(interfaceMap).join(", "));
}

function appendPermissions(permissions, interfaces) {
  for (var entry of interfaces) {
    if (entry.permission !== undefined &&
        permissions.indexOf(entry.permission) === -1) {
      permissions.push(entry.permission);
    }
  }
}

var permissions = [];
appendPermissions(permissions, ecmaGlobals);
appendPermissions(permissions, interfaceNamesInGlobalScope);

workerTestGetPermissions(permissions, function(permissionMap) {
  workerTestGetVersion(function(version) {
    workerTestGetUserAgent(function(userAgent) {
      workerTestGetIsB2G(function(isB2G) {
        runTest(permissionMap, version, userAgent, isB2G);
        workerTestDone();
      });
    });
  });
});
