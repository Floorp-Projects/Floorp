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
//   "AGlobalInterface", // secure context only
//   { name: "ExperimentalThing", release: false },
//   { name: "ReallyExperimentalThing", nightly: true },
//   { name: "DesktopOnlyThing", desktop: true },
//   { name: "FancyControl", xbl: true },
//   { name: "DisabledEverywhere", disabled: true },
// ];
//
// See createInterfaceMap() below for a complete list of properties.
//
// The values of the properties need to be literal true/false
// (e.g. indicating whether something is enabled on a particular
// channel/OS).  If we ever end up in a situation where a propert
// value needs to depend on channel or OS, we will need to make sure
// we have that information before setting up the property lists.

// IMPORTANT: Do not change this list without review from
//            a JavaScript Engine peer!
var ecmaGlobals =
  [
    {name: "Array", insecureContext: true},
    {name: "ArrayBuffer", insecureContext: true},
    {name: "Atomics", insecureContext: true, disabled: true},
    {name: "BigInt", insecureContext: true},
    {name: "BigInt64Array", insecureContext: true},
    {name: "BigUint64Array", insecureContext: true},
    {name: "Boolean", insecureContext: true},
    {name: "ByteLengthQueuingStrategy", insecureContext: true},
    {name: "CountQueuingStrategy", insecureContext: true},
    {name: "DataView", insecureContext: true},
    {name: "Date", insecureContext: true},
    {name: "Error", insecureContext: true},
    {name: "EvalError", insecureContext: true},
    {name: "Float32Array", insecureContext: true},
    {name: "Float64Array", insecureContext: true},
    {name: "Function", insecureContext: true},
    {name: "Infinity", insecureContext: true},
    {name: "Int16Array", insecureContext: true},
    {name: "Int32Array", insecureContext: true},
    {name: "Int8Array", insecureContext: true},
    {name: "InternalError", insecureContext: true},
    {name: "Intl", insecureContext: true},
    {name: "JSON", insecureContext: true},
    {name: "Map", insecureContext: true},
    {name: "MediaCapabilities", insecureContext: true},
    {name: "MediaCapabilitiesInfo", insecureContext: true},
    {name: "Math", insecureContext: true},
    {name: "NaN", insecureContext: true},
    {name: "Number", insecureContext: true},
    {name: "Object", insecureContext: true},
    {name: "Promise", insecureContext: true},
    {name: "Proxy", insecureContext: true},
    {name: "RangeError", insecureContext: true},
    {name: "ReadableStream", insecureContext: true},
    {name: "ReferenceError", insecureContext: true},
    {name: "Reflect", insecureContext: true},
    {name: "RegExp", insecureContext: true},
    {name: "Set", insecureContext: true},
    {name: "SharedArrayBuffer", insecureContext: true, disabled: true},
    {name: "String", insecureContext: true},
    {name: "Symbol", insecureContext: true},
    {name: "SyntaxError", insecureContext: true},
    {name: "TypedObject", insecureContext: true, nightly: true},
    {name: "TypeError", insecureContext: true},
    {name: "Uint16Array", insecureContext: true},
    {name: "Uint32Array", insecureContext: true},
    {name: "Uint8Array", insecureContext: true},
    {name: "Uint8ClampedArray", insecureContext: true},
    {name: "URIError", insecureContext: true},
    {name: "WeakMap", insecureContext: true},
    {name: "WeakSet", insecureContext: true},
    {name: "WebAssembly", insecureContext: true, disabled: !getJSTestingFunctions().wasmIsSupportedByHardware()},
  ];
// IMPORTANT: Do not change the list above without review from
//            a JavaScript Engine peer!

// IMPORTANT: Do not change the list below without review from a DOM peer!
var interfaceNamesInGlobalScope =
  [
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "AbortController", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "AbortSignal", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Blob", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "BroadcastChannel", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Cache", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "CacheStorage", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "CloseEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Crypto", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "CustomEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "DedicatedWorkerGlobalScope", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Directory", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "DOMError", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "DOMException", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "DOMRequest", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "DOMStringList", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ErrorEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Event", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "EventSource", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "EventTarget", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "File", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "FileList", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "FileReader", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "FileReaderSync", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "FormData", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Headers", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBCursor", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBCursorWithValue", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBDatabase", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBFactory", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBIndex", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBKeyRange", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBObjectStore", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBOpenDBRequest", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBRequest", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBTransaction", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "IDBVersionChangeEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ImageBitmap", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ImageBitmapRenderingContext", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ImageData", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "MessageChannel", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "MessageEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "MessagePort", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "NetworkInformation", insecureContext: true, android: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Notification", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "OffscreenCanvas", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Performance", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceEntry", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceMark", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceMeasure", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceObserver", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceObserverEntryList", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceResourceTiming", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PerformanceServerTiming", insecureContext: false},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ProgressEvent", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PushManager", insecureContext: true, fennecOrDesktop: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PushSubscription", insecureContext: true, fennecOrDesktop: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "PushSubscriptionOptions", insecureContext: true, fennecOrDesktop: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Request", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Response", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "ServiceWorkerRegistration", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "StorageManager", fennec: false},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "SubtleCrypto", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "TextDecoder", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "TextEncoder", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "XMLHttpRequest", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "XMLHttpRequestEventTarget", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "XMLHttpRequestUpload", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "URL", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "URLSearchParams", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLActiveInfo", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLBuffer", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLContextEvent", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLFramebuffer", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLProgram", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLRenderbuffer", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLRenderingContext", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLShader", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLShaderPrecisionFormat", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLTexture", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    { name: "WebGLUniformLocation", insecureContext: true, disabled: true },
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "WebSocket", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "Worker", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "WorkerGlobalScope", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "WorkerLocation", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
    {name: "WorkerNavigator", insecureContext: true},
// IMPORTANT: Do not change this list without review from a DOM peer!
  ];
// IMPORTANT: Do not change the list above without review from a DOM peer!

function createInterfaceMap({ isNightly, isRelease, isDesktop, isAndroid, isInsecureContext, isFennec }) {
  var interfaceMap = {};

  function addInterfaces(interfaces)
  {
    for (var entry of interfaces) {
      if (typeof(entry) === "string") {
        interfaceMap[entry] = !isInsecureContext;
      } else {
        ok(!("pref" in entry), "Bogus pref annotation for " + entry.name);
        if ((entry.nightly === !isNightly) ||
            (entry.nightlyAndroid === !(isAndroid && isNightly) && isAndroid) ||
            (entry.desktop === !isDesktop) ||
            (entry.android === !isAndroid && !entry.nightlyAndroid) ||
            (entry.fennecOrDesktop === (isAndroid && !isFennec)) ||
            (entry.fennec === !isFennec) ||
            (entry.release === !isRelease) ||
	    // The insecureContext test is very purposefully converting
	    // entry.insecureContext to boolean, so undefined will convert to
	    // false.  That way entries without an insecureContext annotation
	    // will get treated as "insecureContext: false", which means exposed
	    // only in secure contexts.
            (isInsecureContext && !Boolean(entry.insecureContext)) ||
            entry.disabled) {
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

function runTest(data) {
  var interfaceMap = createInterfaceMap(data);
  for (var name of Object.getOwnPropertyNames(self)) {
    // An interface name should start with an upper case character.
    if (!/^[A-Z]/.test(name)) {
      continue;
    }
    ok(interfaceMap[name],
       "If this is failing: DANGER, are you sure you want to expose the new interface " + name +
       " to all webpages as a property on the worker? Do not make a change to this file without a " +
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

workerTestGetHelperData(function(data) {
  runTest(data);
  workerTestDone();
});
