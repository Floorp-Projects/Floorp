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
var wasmGlobalEntry = {
  name: "WebAssembly",
  insecureContext: true,
  disabled: !getJSTestingFunctions().wasmIsSupportedByHardware(),
};
var wasmGlobalInterfaces = [
  { name: "Module", insecureContext: true },
  { name: "Instance", insecureContext: true },
  { name: "Memory", insecureContext: true },
  { name: "Table", insecureContext: true },
  { name: "Global", insecureContext: true },
  { name: "CompileError", insecureContext: true },
  { name: "LinkError", insecureContext: true },
  { name: "RuntimeError", insecureContext: true },
  {
    name: "Function",
    insecureContext: true,
    nightly: true,
  },
  {
    name: "Exception",
    insecureContext: true,
  },
  {
    name: "Tag",
    insecureContext: true,
  },
];
// IMPORTANT: Do not change this list without review from
//            a JavaScript Engine peer!
var ecmaGlobals = [
  "AggregateError",
  "Array",
  "ArrayBuffer",
  "Atomics",
  "Boolean",
  "BigInt",
  "BigInt64Array",
  "BigUint64Array",
  "DataView",
  "Date",
  "Error",
  "EvalError",
  "FinalizationRegistry",
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
  "ReferenceError",
  "Reflect",
  "RegExp",
  "Set",
  {
    name: "SharedArrayBuffer",
    crossOriginIsolated: true,
  },
  "String",
  "Symbol",
  "SyntaxError",
  "TypeError",
  "Uint16Array",
  "Uint32Array",
  "Uint8Array",
  "Uint8ClampedArray",
  "URIError",
  "WeakMap",
  "WeakRef",
  "WeakSet",
  wasmGlobalEntry,
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
  "ByteLengthQueuingStrategy",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Cache",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "CacheStorage",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasGradient", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasPattern", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Client",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Clients",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "CloseEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "CountQueuingStrategy",
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
  { name: "FontFace", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FontFaceSet", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FontFaceSetLoadEvent", earlyBetaOrEarlier: true },
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
  "Lock",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "LockManager",
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
  { name: "NetworkInformation", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "NavigationPreloadManager",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "Notification",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "NotificationEvent",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OffscreenCanvas", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OffscreenCanvasRenderingContext2D", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Path2D", earlyBetaOrEarlier: true },
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
  "ReadableByteStreamController",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ReadableStream",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ReadableStreamBYOBReader",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ReadableStreamBYOBRequest",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ReadableStreamDefaultController",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "ReadableStreamDefaultReader",
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
  { name: "Scheduler", nightly: true },
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
  { name: "TaskController", nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TaskPriorityChangeEvent", nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TaskSignal", nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "TextDecoder",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "TextEncoder",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "TextMetrics",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "TransformStream",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "TransformStreamDefaultController",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "URL",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "URLSearchParams",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WebSocket",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGL2RenderingContext", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLActiveInfo", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLBuffer", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLContextEvent", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLFramebuffer", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLProgram", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WebGLQuery",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLRenderbuffer", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLRenderingContext", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLSampler", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLShader", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLShaderPrecisionFormat", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLSync", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLTexture", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLTransformFeedback", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLUniformLocation", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLVertexArrayObject", earlyBetaOrEarlier: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WindowClient",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WorkerGlobalScope",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WorkerLocation",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WorkerNavigator",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WritableStream",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WritableStreamDefaultController",
  // IMPORTANT: Do not change this list without review from a DOM peer!
  "WritableStreamDefaultWriter",
];
// IMPORTANT: Do not change the list above without review from a DOM peer!

function entryDisabled(
  entry,
  {
    isNightly,
    isEarlyBetaOrEarlier,
    isRelease,
    isDesktop,
    isAndroid,
    isInsecureContext,
    isFennec,
    isCrossOriginIsolated,
  }
) {
  return (
    entry.nightly === !isNightly ||
    (entry.nightlyAndroid === !(isAndroid && isNightly) && isAndroid) ||
    (entry.nonReleaseAndroid === !(isAndroid && !isRelease) && isAndroid) ||
    entry.desktop === !isDesktop ||
    (entry.android === !isAndroid &&
      !entry.nonReleaseAndroid &&
      !entry.nightlyAndroid) ||
    entry.fennecOrDesktop === (isAndroid && !isFennec) ||
    entry.fennec === !isFennec ||
    entry.release === !isRelease ||
    entry.earlyBetaOrEarlier === !isEarlyBetaOrEarlier ||
    entry.crossOriginIsolated === !isCrossOriginIsolated ||
    entry.disabled
  );
}

function createInterfaceMap(data, ...interfaceGroups) {
  var interfaceMap = {};

  function addInterfaces(interfaces) {
    for (var entry of interfaces) {
      if (typeof entry === "string") {
        interfaceMap[entry] = true;
      } else {
        ok(!("pref" in entry), "Bogus pref annotation for " + entry.name);
        if (entryDisabled(entry, data)) {
          interfaceMap[entry.name] = false;
        } else if (entry.optional) {
          interfaceMap[entry.name] = "optional";
        } else {
          interfaceMap[entry.name] = true;
        }
      }
    }
  }

  for (let interfaceGroup of interfaceGroups) {
    addInterfaces(interfaceGroup);
  }

  return interfaceMap;
}

function runTest(parentName, parent, data, ...interfaceGroups) {
  var interfaceMap = createInterfaceMap(data, ...interfaceGroups);
  for (var name of Object.getOwnPropertyNames(parent)) {
    // An interface name should start with an upper case character.
    if (!/^[A-Z]/.test(name)) {
      continue;
    }
    ok(
      interfaceMap[name] === "optional" || interfaceMap[name],
      "If this is failing: DANGER, are you sure you want to expose the new interface " +
        name +
        " to all webpages as a property on " +
        parentName +
        "? Do not make a change to this file without a " +
        " review from a DOM peer for that specific change!!! (or a JS peer for changes to ecmaGlobals)"
    );
    delete interfaceMap[name];
  }
  for (var name of Object.keys(interfaceMap)) {
    if (interfaceMap[name] === "optional") {
      delete interfaceMap[name];
    } else {
      ok(
        name in parent === interfaceMap[name],
        name +
          " should " +
          (interfaceMap[name] ? "" : " NOT") +
          " be defined on " +
          parentName
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
  runTest("self", self, data, ecmaGlobals, interfaceNamesInGlobalScope);
  if (WebAssembly && !entryDisabled(wasmGlobalEntry, data)) {
    runTest("WebAssembly", WebAssembly, data, wasmGlobalInterfaces);
  }
  workerTestDone();
});
