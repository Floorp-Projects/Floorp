/** Test for Bug 766694 **/

// This is a list of all interfaces that are exposed to every webpage.
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
//   {name: "ExperimentalThing", release: false},
//   {name: "ReallyExperimentalThing", nightly: true},
//   {name: "DesktopOnlyThing", desktop: true},
//   {name: "DisabledEverywhere", disabled: true},
// ];
//
// See createInterfaceMap() below for a complete list of properties.
//
// The values of the properties need to be either literal true/false
// (e.g. indicating whether something is enabled on a particular
// channel/OS) or one of the is* constants below (in cases when
// exposure is affected by channel or OS in a nontrivial way).

const { AppConstants } = SpecialPowers.Cu.import(
  "resource://gre/modules/AppConstants.jsm",
  {}
);

const isNightly = AppConstants.NIGHTLY_BUILD;
const isEarlyBetaOrEarlier = AppConstants.EARLY_BETA_OR_EARLIER;
const isRelease = AppConstants.RELEASE_OR_BETA;
const isDesktop = !/Mobile|Tablet/.test(navigator.userAgent);
const isMac = AppConstants.platform == "macosx";
const isWindows = AppConstants.platform == "win";
const isAndroid = AppConstants.platform == "android";
const isLinux = AppConstants.platform == "linux";
const isInsecureContext = !window.isSecureContext;
// Currently, MOZ_APP_NAME is always "fennec" for all mobile builds, so we can't use AppConstants for this
const isFennec =
  isAndroid &&
  SpecialPowers.Cc["@mozilla.org/android/bridge;1"].getService(
    SpecialPowers.Ci.nsIAndroidBridge
  ).isFennec;
const isCrossOriginIsolated = window.crossOriginIsolated;

// IMPORTANT: Do not change this list without review from
//            a JavaScript Engine peer!
var ecmaGlobals = [
  { name: "AggregateError", insecureContext: true },
  { name: "Array", insecureContext: true },
  { name: "ArrayBuffer", insecureContext: true },
  { name: "Atomics", insecureContext: true },
  { name: "BigInt", insecureContext: true },
  { name: "BigInt64Array", insecureContext: true },
  { name: "BigUint64Array", insecureContext: true },
  { name: "Boolean", insecureContext: true },
  { name: "ByteLengthQueuingStrategy", insecureContext: true },
  { name: "CountQueuingStrategy", insecureContext: true },
  { name: "DataView", insecureContext: true },
  { name: "Date", insecureContext: true },
  { name: "Error", insecureContext: true },
  { name: "EvalError", insecureContext: true },
  { name: "FinalizationRegistry", insecureContext: true },
  { name: "Float32Array", insecureContext: true },
  { name: "Float64Array", insecureContext: true },
  { name: "Function", insecureContext: true },
  { name: "Infinity", insecureContext: true },
  { name: "Int16Array", insecureContext: true },
  { name: "Int32Array", insecureContext: true },
  { name: "Int8Array", insecureContext: true },
  { name: "InternalError", insecureContext: true },
  { name: "Intl", insecureContext: true },
  { name: "JSON", insecureContext: true },
  { name: "Map", insecureContext: true },
  { name: "Math", insecureContext: true },
  { name: "NaN", insecureContext: true },
  { name: "Number", insecureContext: true },
  { name: "Object", insecureContext: true },
  { name: "Promise", insecureContext: true },
  { name: "Proxy", insecureContext: true },
  { name: "RangeError", insecureContext: true },
  { name: "ReadableStream", insecureContext: true },
  { name: "ReferenceError", insecureContext: true },
  { name: "Reflect", insecureContext: true },
  { name: "RegExp", insecureContext: true },
  { name: "Set", insecureContext: true },
  {
    name: "SharedArrayBuffer",
    insecureContext: true,
    crossOriginIsolated: true,
  },
  { name: "String", insecureContext: true },
  { name: "Symbol", insecureContext: true },
  { name: "SyntaxError", insecureContext: true },
  { name: "TypeError", insecureContext: true },
  { name: "Uint16Array", insecureContext: true },
  { name: "Uint32Array", insecureContext: true },
  { name: "Uint8Array", insecureContext: true },
  { name: "Uint8ClampedArray", insecureContext: true },
  { name: "URIError", insecureContext: true },
  { name: "WeakMap", insecureContext: true },
  { name: "WeakRef", insecureContext: true },
  { name: "WeakSet", insecureContext: true },
  {
    name: "WebAssembly",
    insecureContext: true,
    disabled: !SpecialPowers.Cu.getJSTestingFunctions().wasmIsSupportedByHardware(),
  },
];
// IMPORTANT: Do not change the list above without review from
//            a JavaScript Engine peer!

// IMPORTANT: Do not change the list below without review from a DOM peer,
//            except to remove items from it!
//
// This is a list of interfaces that were prefixed with 'moz' instead of 'Moz'.
// We should never to that again, interfaces in the DOM start with an uppercase
// letter. If you think you need to add an interface here, DON'T. Rename your
// interface.
var legacyMozPrefixedInterfaces = [
  "mozContact",
  "mozRTCIceCandidate",
  "mozRTCPeerConnection",
  "mozRTCSessionDescription",
];
// IMPORTANT: Do not change the list above without review from a DOM peer,
//            except to remove items from it!

// IMPORTANT: Do not change the list below without review from a DOM peer!
var interfaceNamesInGlobalScope = [
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AbortController", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AbortSignal", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AbstractRange", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AnalyserNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Animation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AnimationEffect", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AnimationEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AnimationPlaybackEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AnimationTimeline", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Attr", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Audio", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioBuffer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioBufferSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioDestinationNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioListener", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioParam", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioParamMap", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioProcessingEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioScheduledSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioWorklet", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AudioWorkletNode", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AuthenticatorAssertionResponse" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AuthenticatorAttestationResponse" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "AuthenticatorResponse" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BarProp", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BaseAudioContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BeforeUnloadEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BiquadFilterNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Blob", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BlobEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "BroadcastChannel", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Cache", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CacheStorage", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasCaptureMediaStream", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasGradient", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasPattern", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CanvasRenderingContext2D", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CaretPosition", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CDATASection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ChannelMergerNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ChannelSplitterNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CharacterData", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Clipboard" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ClipboardEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CloseEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Comment", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CompositionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ConstantSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ConvolverNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Credential" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CredentialsContainer" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Crypto", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CryptoKey" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSS", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSS2Properties", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSAnimation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSConditionRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSCounterStyleRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSFontFaceRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSFontFeatureValuesRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSGroupingRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSImportRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSKeyframeRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSKeyframesRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSMediaRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSMozDocumentRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSNamespaceRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSPageRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSPseudoElement", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSRuleList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSStyleDeclaration", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSStyleRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSStyleSheet", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSSupportsRule", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CSSTransition", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CustomElementRegistry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "CustomEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DataTransfer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DataTransferItem", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DataTransferItemList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DelayNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DeprecationReportBody", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DeviceLightEvent", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DeviceMotionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DeviceOrientationEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DeviceProximityEvent", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Directory", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Document", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DocumentFragment", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DocumentTimeline", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DocumentType", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMException", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMImplementation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMMatrix", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMMatrixReadOnly", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMParser", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMPoint", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMPointReadOnly", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMQuad", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMRect", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMRectList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMRectReadOnly", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMStringList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMStringMap", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DOMTokenList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DragEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "DynamicsCompressorNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Element", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ElementInternals", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ErrorEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Event", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "EventCounts", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "EventSource", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "EventTarget", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "FeaturePolicyViolationReportBody",
    insecureContext: true,
    nightly: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "File", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileReader", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileSystem", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileSystemDirectoryEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileSystemDirectoryReader", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileSystemEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FileSystemFileEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FocusEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FormData", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FormDataEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FontFace", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FontFaceSet", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "FontFaceSetLoadEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GainNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Gamepad", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadAxisMoveEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadButtonEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadButton", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadHapticActuator", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadLightIndicator", insecureContext: false, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadPose", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GamepadTouch", insecureContext: false, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Geolocation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GeolocationCoordinates", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GeolocationPosition", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "GeolocationPositionError", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HashChangeEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Headers", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "History", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLAllCollection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLAnchorElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLAreaElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLAudioElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLBaseElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLBodyElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLBRElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLButtonElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLCanvasElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLCollection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDataElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDataListElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDetailsElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDialogElement", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDirectoryElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDivElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDListElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLDocument", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLEmbedElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFieldSetElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFontElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFormControlsCollection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFormElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFrameElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLFrameSetElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLHeadElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLHeadingElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLHRElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLHtmlElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLIFrameElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLImageElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLInputElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLLabelElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLLegendElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLLIElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLLinkElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMapElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMarqueeElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMediaElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMenuElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMetaElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLMeterElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLModElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLObjectElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLOListElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLOptGroupElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLOptionElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLOptionsCollection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLOutputElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLParagraphElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLParamElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLPreElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLPictureElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLProgressElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLQuoteElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLScriptElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLSelectElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLSlotElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLSourceElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLSpanElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLStyleElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableCaptionElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableCellElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableColElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableRowElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTableSectionElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTemplateElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTextAreaElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTimeElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTitleElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLTrackElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLUListElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLUnknownElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "HTMLVideoElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IdleDeadline", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBCursor", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBCursorWithValue", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBDatabase", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBFactory", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBFileHandle", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBFileRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBIndex", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBKeyRange", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBMutableFile", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBObjectStore", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBOpenDBRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBTransaction", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IDBVersionChangeEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IIRFilterNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Image", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ImageBitmap", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ImageBitmapRenderingContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ImageCapture", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ImageCaptureErrorEvent", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ImageData", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "InputEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "InstallTrigger", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IntersectionObserver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "IntersectionObserverEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "KeyEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "KeyboardEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "KeyframeEffect", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Location", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MathMLElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaCapabilities", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaCapabilitiesInfo", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaDeviceInfo", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaDevices", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaElementAudioSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaError", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeyError", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaEncryptedEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeys", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeySession", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeySystemAccess", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeyMessageEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaKeyStatusMap", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaMetadata", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaQueryList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaQueryListEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaRecorder", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaRecorderErrorEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaSession", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaSource", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStream", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamAudioDestinationNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamAudioSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamTrackAudioSourceNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamTrackEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MediaStreamTrack", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "MerchantValidationEvent",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MessageChannel", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MessageEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MessagePort", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIAccess", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIConnectionEvent", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIInputMap", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIInput", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIMessageEvent", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIOutputMap", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIOutput", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MIDIPort", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MimeType", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MimeTypeArray", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MouseEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MouseScrollEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "mozRTCIceCandidate", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "mozRTCPeerConnection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "mozRTCSessionDescription", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MutationEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MutationObserver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "MutationRecord", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NamedNodeMap", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Navigator", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NetworkInformation", insecureContext: true, desktop: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Node", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NodeFilter", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NodeIterator", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "NodeList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Notification", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OffscreenCanvas", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OfflineAudioCompletionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OfflineAudioContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "OfflineResourceList",
    insecureContext: false,
    disabled: isEarlyBetaOrEarlier,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Option", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "OscillatorNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PageTransitionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PaintRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PaintRequestList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PannerNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Path2D", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PaymentAddress",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PaymentMethodChangeEvent",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PaymentRequest",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PaymentRequestUpdateEvent",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PaymentResponse",
    insecureContext: false,
    desktop: true,
    nightly: true,
    linux: false,
    disabled: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Performance", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceEventTiming", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceMark", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceMeasure", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceNavigation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceNavigationTiming", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceObserver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceObserverEntryList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformancePaintTiming", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceResourceTiming", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceServerTiming", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PerformanceTiming", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PeriodicWave", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Permissions", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PermissionStatus", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Plugin", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PluginArray", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PointerEvent", insecureContext: true, fennec: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PopStateEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PopupBlockedEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "Presentation",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationAvailability",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationConnection",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationConnectionAvailableEvent",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationConnectionCloseEvent",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationConnectionList",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationReceiver",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PresentationRequest",
    insecureContext: true,
    desktop: false,
    release: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ProcessingInstruction", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ProgressEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PromiseRejectionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PublicKeyCredential" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushManager", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "PushSubscription", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "PushSubscriptionOptions",
    insecureContext: true,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RadioNodeList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Range", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Report", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ReportBody", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ReportingObserver", insecureContext: true, nightly: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Request", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ResizeObserver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ResizeObserverEntry", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ResizeObserverSize", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Response", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCCertificate", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCDataChannel", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCDataChannelEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCDtlsTransport", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCDTMFSender", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCDTMFToneChangeEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCIceCandidate", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCPeerConnection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCPeerConnectionIceEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCRtpReceiver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCRtpSender", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCRtpTransceiver", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCSessionDescription", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCStatsReport", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "RTCTrackEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Sanitizer", disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Screen", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ScreenOrientation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ScriptProcessorNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ScrollAreaEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SecurityPolicyViolationEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Selection", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ServiceWorker", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ServiceWorkerContainer", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ServiceWorkerRegistration", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ScopedCredential", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ScopedCredentialInfo", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ShadowRoot", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SharedWorker", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SimpleTest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SourceBuffer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SourceBufferList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpeechSynthesisErrorEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpeechSynthesisEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpeechSynthesis", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpeechSynthesisUtterance", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpeechSynthesisVoice", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SpecialPowers", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StaticRange", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StereoPannerNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Storage", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StorageEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StorageManager", fennec: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StyleSheet", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "StyleSheetList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SubtleCrypto" },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SubmitEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAngle", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedAngle", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedBoolean", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedEnumeration", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedInteger", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedLength", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedLengthList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedNumber", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedNumberList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedPreserveAspectRatio", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedRect", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedString", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimatedTransformList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimateElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimateMotionElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimateTransformElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGAnimationElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGCircleElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGClipPathElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGComponentTransferFunctionElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGDefsElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGDescElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGEllipseElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEBlendElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEColorMatrixElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEComponentTransferElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFECompositeElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEConvolveMatrixElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEDiffuseLightingElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEDisplacementMapElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEDistantLightElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEDropShadowElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEFloodElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEFuncAElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEFuncBElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEFuncGElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEFuncRElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEGaussianBlurElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEImageElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEMergeElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEMergeNodeElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEMorphologyElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEOffsetElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFEPointLightElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFESpecularLightingElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFESpotLightElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFETileElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFETurbulenceElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGFilterElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGForeignObjectElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGGElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGGeometryElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGGradientElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGGraphicsElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGImageElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGLength", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGLengthList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGLinearGradientElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGLineElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGMarkerElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGMaskElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGMatrix", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGMetadataElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGMPathElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGNumber", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGNumberList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPathElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPathSegList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPatternElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPoint", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPointList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPolygonElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPolylineElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGPreserveAspectRatio", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGRadialGradientElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGRect", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGRectElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGScriptElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGSetElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGStopElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGStringList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGStyleElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGSVGElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGSwitchElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGSymbolElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTextContentElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTextElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTextPathElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTextPositioningElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTitleElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTransform", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTransformList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGTSpanElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGUnitTypes", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGUseElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "SVGViewElement", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Text", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextDecoder", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextEncoder", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextMetrics", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextTrack", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextTrackCue", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextTrackCueList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TextTrackList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TimeEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TimeRanges", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Touch", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TouchEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TouchList", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TrackEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TransitionEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "TreeWalker", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "U2F", insecureContext: false, android: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "UIEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "URL", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "URLSearchParams", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "UserProximityEvent", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "ValidityState", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VideoPlaybackQuality", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VisualViewport", insecureContext: true, android: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VRDisplay", releaseNonWindowsAndMac: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VRDisplayCapabilities",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VRDisplayEvent",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VREyeParameters",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VRFieldOfView",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VRFrameData",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VRPose", releaseNonWindowsAndMac: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  {
    name: "VRStageParameters",
    releaseNonWindowsAndMac: false,
  },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VTTCue", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "VTTRegion", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WaveShaperNode", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebAuthnAssertion", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebAuthnAttestation", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebAuthentication", insecureContext: true, disabled: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLActiveInfo", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLBuffer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLContextEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLFramebuffer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLProgram", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLQuery", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLRenderbuffer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLRenderingContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGL2RenderingContext", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLSampler", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLShader", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLShaderPrecisionFormat", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLSync", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLTexture", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLTransformFeedback", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLUniformLocation", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebGLVertexArrayObject", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebKitCSSMatrix", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WebSocket", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "WheelEvent", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Window", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Worker", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "Worklet", insecureContext: false },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XMLDocument", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XMLHttpRequest", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XMLHttpRequestEventTarget", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XMLHttpRequestUpload", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XMLSerializer", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XPathEvaluator", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XPathExpression", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XPathResult", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
  { name: "XSLTProcessor", insecureContext: true },
  // IMPORTANT: Do not change this list without review from a DOM peer!
];
// IMPORTANT: Do not change the list above without review from a DOM peer!

function createInterfaceMap() {
  var interfaceMap = {};

  function addInterfaces(interfaces) {
    for (var entry of interfaces) {
      if (typeof entry === "string") {
        interfaceMap[entry] = !isInsecureContext;
      } else {
        ok(!("pref" in entry), "Bogus pref annotation for " + entry.name);
        if (
          entry.nightly === !isNightly ||
          (entry.nightlyAndroid === !(isAndroid && isNightly) && isAndroid) ||
          entry.desktop === !isDesktop ||
          entry.windows === !isWindows ||
          entry.mac === !isMac ||
          entry.linux === !isLinux ||
          (entry.android === !isAndroid && !entry.nightlyAndroid) ||
          entry.fennecOrDesktop === (isAndroid && !isFennec) ||
          entry.fennec === !isFennec ||
          entry.release === !isRelease ||
          entry.releaseNonWindowsAndMac ===
            !(isRelease && !isWindows && !isMac) ||
          entry.releaseNonWindows === !(isRelease && !isWindows) ||
          // The insecureContext test is very purposefully converting
          // entry.insecureContext to boolean, so undefined will convert to
          // false.  That way entries without an insecureContext annotation
          // will get treated as "insecureContext: false", which means exposed
          // only in secure contexts.
          (isInsecureContext && !entry.insecureContext) ||
          entry.earlyBetaOrEarlier === !isEarlyBetaOrEarlier ||
          entry.crossOriginIsolated === !isCrossOriginIsolated ||
          entry.disabled
        ) {
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

function runTest() {
  var interfaceMap = createInterfaceMap();
  for (var name of Object.getOwnPropertyNames(window)) {
    // An interface name should start with an upper case character.
    // However, we have a couple of legacy interfaces that start with 'moz', so
    // we want to allow those until we can remove them.
    if (!/^[A-Z]/.test(name) && !legacyMozPrefixedInterfaces.includes(name)) {
      continue;
    }
    ok(
      interfaceMap[name],
      "If this is failing: DANGER, are you sure you want to expose the new interface " +
        name +
        " to all webpages as a property on the window? Do not make a change to this file without a " +
        " review from a DOM peer for that specific change!!! (or a JS peer for changes to ecmaGlobals)"
    );

    ok(
      name in window,
      `${name} is exposed as an own property on the window but tests false for "in" in the global scope`
    );
    ok(
      Object.getOwnPropertyDescriptor(window, name),
      `${name} is exposed as an own property on the window but has no property descriptor in the global scope`
    );

    delete interfaceMap[name];
  }
  for (var name of Object.keys(interfaceMap)) {
    ok(
      name in window === interfaceMap[name],
      name +
        " should " +
        (interfaceMap[name] ? "" : " NOT") +
        " be defined on the global scope"
    );
    if (!interfaceMap[name]) {
      delete interfaceMap[name];
    }
  }
  is(
    Object.keys(interfaceMap).length,
    0,
    "The following interface(s) are not enumerated: " +
      Object.keys(interfaceMap).join(", ")
  );
}

runTest();
