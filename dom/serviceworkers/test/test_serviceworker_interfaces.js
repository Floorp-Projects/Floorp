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
//   { name: "FancyControl", xbl: true },
//   { name: "DisabledEverywhere", disabled: true },
// ];
//
// See createInterfaceMap() below for a complete list of properties.

// IMPORTANT: Do not change this list without review from
//            a JavaScript Engine peer!
var ecmaGlobals = [
  { name: "AggregateError", nightly: true },
  "Array",
  "ArrayBuffer",
  { name: "Atomics", earlyBetaOrEarlier: true },
  "Boolean",
  "BigInt",
  "BigInt64Array",
  "BigUint64Array",
  { name: "ByteLengthQueuingStrategy", optional: true },
  { name: "CountQueuingStrategy", optional: true },
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
  "Intl",
  "JSON",
  "Map",
  "Math",
  "NaN",
  "Number",
  "Object",
  "Promise",
  "Proxy",
  "RangeError",
  { name: "ReadableStream", optional: true },
  "ReferenceError",
  "Reflect",
  "RegExp",
  "Set",
  { name: "SharedArrayBuffer", earlyBetaOrEarlier: true },
  "String",
  "Symbol",
  "SyntaxError",
  { name: "TypedObject", nightly: true },
  "TypeError",
  "Uint16Array",
  "Uint32Array",
  "Uint8Array",
  "Uint8ClampedArray",
  "URIError",
  "WeakMap",
  "WeakSet",
  { name: "WebAssembly", optional: true },
];
// IMPORTANT: Do not change the list above without review from
//            a JavaScript Engine peer!

// IMPORTANT: Do not change the list below without review from a DOM peer!
var interfaceNamesInGlobalScope = [
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "AbortController",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "AbortSignal",
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
  "CloseEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Crypto",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "CustomEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Directory",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMException",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMMatrix",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMMatrixReadOnly",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMPoint",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMPointReadOnly",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMQuad",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMRect",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMRectReadOnly",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMRequest",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "DOMStringList",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ErrorEvent",
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
  "FileList",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "FileReader",
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
  "MediaCapabilities",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "MediaCapabilitiesInfo",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "MessageChannel",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "MessageEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "MessagePort",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NetworkInformation", android: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Notification",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "NotificationEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Performance",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceEntry",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceMark",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceMeasure",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceObserver",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceObserverEntryList",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceResourceTiming",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PerformanceServerTiming",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ProgressEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "PromiseRejectionEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushEvent" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushManager" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushMessageData" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushSubscription" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushSubscriptionOptions" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Report", nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ReportBody", nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ReportingObserver", nightly: true },
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
  { name: "StorageManager", fennec: false },
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

function createInterfaceMap({
  isNightly,
  isEarlyBetaOrEarlier,
  isRelease,
  isDesktop,
  isAndroid,
  isInsecureContext,
  isFennec,
}) {
  var interfaceMap = {};

  function addInterfaces(interfaces) {
    for (var entry of interfaces) {
      if (typeof entry === "string") {
        interfaceMap[entry] = true;
      } else {
        ok(!("pref" in entry), "Bogus pref annotation for " + entry.name);
        if (
          entry.nightly === !isNightly ||
          (entry.nightlyAndroid === !(isAndroid && isNightly) && isAndroid) ||
          (entry.nonReleaseAndroid === !(isAndroid && !isRelease) &&
            isAndroid) ||
          entry.desktop === !isDesktop ||
          (entry.android === !isAndroid &&
            !entry.nonReleaseAndroid &&
            !entry.nightlyAndroid) ||
          entry.fennecOrDesktop === (isAndroid && !isFennec) ||
          entry.fennec === !isFennec ||
          entry.release === !isRelease ||
          entry.earlyBetaOrEarlier === !isEarlyBetaOrEarlier ||
          entry.disabled
        ) {
          interfaceMap[entry.name] = false;
        } else if (entry.optional) {
          interfaceMap[entry.name] = "optional";
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

function runTest(data) {
  var interfaceMap = createInterfaceMap(data);
  for (var name of Object.getOwnPropertyNames(self)) {
    // An interface name should start with an upper case character.
    if (!/^[A-Z]/.test(name)) {
      continue;
    }
    ok(
      interfaceMap[name] === "optional" || interfaceMap[name],
      "If this is failing: DANGER, are you sure you want to expose the new interface " +
        name +
        " to all webpages as a property on the service worker? Do not make a change to this file without a " +
        " review from a DOM peer for that specific change!!! (or a JS peer for changes to ecmaGlobals)"
    );
    delete interfaceMap[name];
  }
  for (var name of Object.keys(interfaceMap)) {
    if (interfaceMap[name] === "optional") {
      delete interfaceMap[name];
    } else {
      ok(
        name in self === interfaceMap[name],
        name +
          " should " +
          (interfaceMap[name] ? "" : " NOT") +
          " be defined on the global scope"
      );
      if (!interfaceMap[name]) {
        delete interfaceMap[name];
      }
    }
  }
  is(
    Object.keys(interfaceMap).length,
    0,
    "The following interface(s) are not enumerated: " +
      Object.keys(interfaceMap).join(", ")
  );
}

workerTestGetHelperData(function(data) {
  runTest(data);
  workerTestDone();
});
