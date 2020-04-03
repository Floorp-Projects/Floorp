/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * http://www.whatwg.org/specs/web-apps/current-work/
 * https://dvcs.w3.org/hg/editing/raw-file/tip/editing.html
 * https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html
 * http://dev.w3.org/csswg/cssom/
 * http://dev.w3.org/csswg/cssom-view/
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/RequestAnimationFrame/Overview.html
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html
 * https://dvcs.w3.org/hg/webcrypto-api/raw-file/tip/spec/Overview.html
 * http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
 * https://w3c.github.io/webappsec-secure-contexts/#monkey-patching-global-object
 * https://w3c.github.io/requestidlecallback/
 * https://drafts.css-houdini.org/css-paint-api-1/#dom-window-paintworklet
 * https://wicg.github.io/visual-viewport/#the-visualviewport-interface
 */

interface nsIBrowserDOMWindow;
interface XULControllers;
interface nsIDOMWindowUtils;

typedef OfflineResourceList ApplicationCache;

// http://www.whatwg.org/specs/web-apps/current-work/
[Global, LegacyUnenumerableNamedProperties, NeedResolve,
 Exposed=Window,
 InstrumentedProps=(AbsoluteOrientationSensor,
                    Accelerometer,
                    ApplicationCache,
                    ApplicationCacheErrorEvent,
                    Atomics,
                    AudioParamMap,
                    AudioWorklet,
                    AudioWorkletNode,
                    BackgroundFetchManager,
                    BackgroundFetchRecord,
                    BackgroundFetchRegistration,
                    BeforeInstallPromptEvent,
                    Bluetooth,
                    BluetoothCharacteristicProperties,
                    BluetoothDevice,
                    BluetoothRemoteGATTCharacteristic,
                    BluetoothRemoteGATTDescriptor,
                    BluetoothRemoteGATTServer,
                    BluetoothRemoteGATTService,
                    BluetoothUUID,
                    CanvasCaptureMediaStreamTrack,
                    chrome,
                    clientInformation,
                    ClipboardItem,
                    CSSImageValue,
                    CSSKeywordValue,
                    CSSMathInvert,
                    CSSMathMax,
                    CSSMathMin,
                    CSSMathNegate,
                    CSSMathProduct,
                    CSSMathSum,
                    CSSMathValue,
                    CSSMatrixComponent,
                    CSSNumericArray,
                    CSSNumericValue,
                    CSSPerspective,
                    CSSPositionValue,
                    CSSRotate,
                    CSSScale,
                    CSSSkew,
                    CSSSkewX,
                    CSSSkewY,
                    CSSStyleValue,
                    CSSTransformComponent,
                    CSSTransformValue,
                    CSSTranslate,
                    CSSUnitValue,
                    CSSUnparsedValue,
                    CSSVariableReferenceValue,
                    defaultStatus,
                    // Unfortunately, our telemetry histogram name generator
                    // (the one that generates TelemetryHistogramEnums.h) can't
                    // handle two DOM methods with names that only differ in
                    // case, because it forces everything to uppercase.
                    //defaultstatus,
                    DeviceMotionEventAcceleration,
                    DeviceMotionEventRotationRate,
                    DOMError,
                    EnterPictureInPictureEvent,
                    External,
                    FederatedCredential,
                    Gyroscope,
                    HTMLContentElement,
                    HTMLDialogElement,
                    HTMLShadowElement,
                    ImageCapture,
                    InputDeviceCapabilities,
                    InputDeviceInfo,
                    Keyboard,
                    KeyboardLayoutMap,
                    LinearAccelerationSensor,
                    Lock,
                    LockManager,
                    MediaMetadata,
                    MediaSession,
                    MediaSettingsRange,
                    MIDIAccess,
                    MIDIConnectionEvent,
                    MIDIInput,
                    MIDIInputMap,
                    MIDIMessageEvent,
                    MIDIOutput,
                    MIDIOutputMap,
                    MIDIPort,
                    NavigationPreloadManager,
                    NetworkInformation,
                    offscreenBuffering,
                    OffscreenCanvas,
                    OffscreenCanvasRenderingContext2D,
                    onbeforeinstallprompt,
                    oncancel,
                    ondeviceorientationabsolute,
                    onmousewheel,
                    onsearch,
                    onselectionchange,
                    openDatabase,
                    OrientationSensor,
                    OverconstrainedError,
                    PasswordCredential,
                    PaymentAddress,
                    PaymentInstruments,
                    PaymentManager,
                    PaymentMethodChangeEvent,
                    PaymentRequest,
                    PaymentRequestUpdateEvent,
                    PaymentResponse,
                    PerformanceEventTiming,
                    PerformanceLongTaskTiming,
                    PerformancePaintTiming,
                    PhotoCapabilities,
                    PictureInPictureWindow,
                    Presentation,
                    PresentationAvailability,
                    PresentationConnection,
                    PresentationConnectionAvailableEvent,
                    PresentationConnectionCloseEvent,
                    PresentationConnectionList,
                    PresentationReceiver,
                    PresentationRequest,
                    RelativeOrientationSensor,
                    RemotePlayback,
                    ReportingObserver,
                    RTCDtlsTransport,
                    RTCError,
                    RTCErrorEvent,
                    RTCIceTransport,
                    RTCSctpTransport,
                    Sensor,
                    SensorErrorEvent,
                    SharedArrayBuffer,
                    styleMedia,
                    StylePropertyMap,
                    StylePropertyMapReadOnly,
                    SVGDiscardElement,
                    SyncManager,
                    TaskAttributionTiming,
                    TextDecoderStream,
                    TextEncoderStream,
                    TextEvent,
                    Touch,
                    TouchEvent,
                    TouchList,
                    TransformStream,
                    USB,
                    USBAlternateInterface,
                    USBConfiguration,
                    USBConnectionEvent,
                    USBDevice,
                    USBEndpoint,
                    USBInterface,
                    USBInTransferResult,
                    USBIsochronousInTransferPacket,
                    USBIsochronousInTransferResult,
                    USBIsochronousOutTransferPacket,
                    USBIsochronousOutTransferResult,
                    USBOutTransferResult,
                    UserActivation,
                    visualViewport,
                    webkitCancelAnimationFrame,
                    webkitMediaStream,
                    WebKitMutationObserver,
                    webkitRequestAnimationFrame,
                    webkitRequestFileSystem,
                    webkitResolveLocalFileSystemURL,
                    webkitRTCPeerConnection,
                    webkitSpeechGrammar,
                    webkitSpeechGrammarList,
                    webkitSpeechRecognition,
                    webkitSpeechRecognitionError,
                    webkitSpeechRecognitionEvent,
                    webkitStorageInfo,
                    Worklet,
                    WritableStream)]
/*sealed*/ interface Window : EventTarget {
  // the current browsing context
  [Unforgeable, Constant, StoreInSlot,
   CrossOriginReadable] readonly attribute WindowProxy window;
  [Replaceable, Constant, StoreInSlot,
   CrossOriginReadable] readonly attribute WindowProxy self;
  [Unforgeable, StoreInSlot, Pure] readonly attribute Document? document;
  [Throws] attribute DOMString name;
  [PutForwards=href, Unforgeable, CrossOriginReadable,
   CrossOriginWritable] readonly attribute Location location;
  [Throws] readonly attribute History history;
  readonly attribute CustomElementRegistry customElements;
  [Replaceable, Throws] readonly attribute BarProp locationbar;
  [Replaceable, Throws] readonly attribute BarProp menubar;
  [Replaceable, Throws] readonly attribute BarProp personalbar;
  [Replaceable, Throws] readonly attribute BarProp scrollbars;
  [Replaceable, Throws] readonly attribute BarProp statusbar;
  [Replaceable, Throws] readonly attribute BarProp toolbar;
  [Throws] attribute DOMString status;
  [Throws, CrossOriginCallable, NeedsCallerType] void close();
  [Throws, CrossOriginReadable] readonly attribute boolean closed;
  [Throws] void stop();
  [Throws, CrossOriginCallable, NeedsCallerType] void focus();
  [Throws, CrossOriginCallable] void blur();
  [Replaceable, Pref="dom.window.event.enabled"] readonly attribute any event;

  // other browsing contexts
  [Replaceable, Throws, CrossOriginReadable] readonly attribute WindowProxy frames;
  [Replaceable, CrossOriginReadable] readonly attribute unsigned long length;
  //[Unforgeable, Throws, CrossOriginReadable] readonly attribute WindowProxy top;
  [Unforgeable, Throws, CrossOriginReadable] readonly attribute WindowProxy? top;
  [Throws, CrossOriginReadable] attribute any opener;
  //[Throws] readonly attribute WindowProxy parent;
  [Replaceable, Throws, CrossOriginReadable] readonly attribute WindowProxy? parent;
  [Throws, NeedsSubjectPrincipal] readonly attribute Element? frameElement;
  //[Throws] WindowProxy? open(optional USVString url = "about:blank", optional DOMString target = "_blank", [TreatNullAs=EmptyString] optional DOMString features = "");
  [Throws] WindowProxy? open(optional USVString url = "", optional DOMString target = "", optional [TreatNullAs=EmptyString] DOMString features = "");
  getter object (DOMString name);

  // the user agent
  readonly attribute Navigator navigator;
#ifdef HAVE_SIDEBAR
  [Replaceable, Throws] readonly attribute External external;
#endif
  [Throws, SecureContext, Pref="browser.cache.offline.enable"] readonly attribute ApplicationCache applicationCache;

  // user prompts
  [Throws, NeedsSubjectPrincipal] void alert();
  [Throws, NeedsSubjectPrincipal] void alert(DOMString message);
  [Throws, NeedsSubjectPrincipal] boolean confirm(optional DOMString message = "");
  [Throws, NeedsSubjectPrincipal] DOMString? prompt(optional DOMString message = "", optional DOMString default = "");
  [Throws, Pref="dom.enable_window_print"]
  void print();

  [Throws, CrossOriginCallable, NeedsSubjectPrincipal,
   BinaryName="postMessageMoz"]
  void postMessage(any message, DOMString targetOrigin, optional sequence<object> transfer = []);
  [Throws, CrossOriginCallable, NeedsSubjectPrincipal,
   BinaryName="postMessageMoz"]
  void postMessage(any message, optional WindowPostMessageOptions options = {});

  // also has obsolete members
};
Window includes GlobalEventHandlers;
Window includes WindowEventHandlers;

// http://www.whatwg.org/specs/web-apps/current-work/
interface mixin WindowSessionStorage {
  //[Throws] readonly attribute Storage sessionStorage;
  [Throws] readonly attribute Storage? sessionStorage;
};
Window includes WindowSessionStorage;

// http://www.whatwg.org/specs/web-apps/current-work/
interface mixin WindowLocalStorage {
  [Throws] readonly attribute Storage? localStorage;
};
Window includes WindowLocalStorage;

// http://www.whatwg.org/specs/web-apps/current-work/
partial interface Window {
  void captureEvents();
  void releaseEvents();
};

// https://dvcs.w3.org/hg/editing/raw-file/tip/editing.html
partial interface Window {
  //[Throws] Selection getSelection();
  [Throws] Selection? getSelection();
};

// http://dev.w3.org/csswg/cssom/
partial interface Window {
  //[NewObject, Throws] CSSStyleDeclaration getComputedStyle(Element elt, optional DOMString pseudoElt = "");
  [NewObject, Throws] CSSStyleDeclaration? getComputedStyle(Element elt, optional DOMString pseudoElt = "");
};

// http://dev.w3.org/csswg/cssom-view/
enum ScrollBehavior { "auto", "instant", "smooth" };

dictionary ScrollOptions {
  ScrollBehavior behavior = "auto";
};

dictionary ScrollToOptions : ScrollOptions {
  unrestricted double left;
  unrestricted double top;
};

partial interface Window {
  //[Throws, NewObject, NeedsCallerType] MediaQueryList matchMedia(DOMString query);
  [Throws, NewObject, NeedsCallerType] MediaQueryList? matchMedia(DOMString query);
  // Per spec, screen is SameObject, but we don't actually guarantee that given
  // nsGlobalWindow::Cleanup.  :(
  //[SameObject, Replaceable, Throws] readonly attribute Screen screen;
  [Replaceable, Throws] readonly attribute Screen screen;

  // browsing context
  //[Throws] void moveTo(double x, double y);
  //[Throws] void moveBy(double x, double y);
  //[Throws] void resizeTo(double x, double y);
  //[Throws] void resizeBy(double x, double y);
  [Throws, NeedsCallerType] void moveTo(long x, long y);
  [Throws, NeedsCallerType] void moveBy(long x, long y);
  [Throws, NeedsCallerType] void resizeTo(long x, long y);
  [Throws, NeedsCallerType] void resizeBy(long x, long y);

  // viewport
  // These are writable because we allow chrome to write them.  And they need
  // to use 'any' as the type, because non-chrome writing them needs to act
  // like a [Replaceable] attribute would, which needs the original JS value.
  //[Replaceable, Throws] readonly attribute double innerWidth;
  //[Replaceable, Throws] readonly attribute double innerHeight;
  [Throws, NeedsCallerType] attribute any innerWidth;
  [Throws, NeedsCallerType] attribute any innerHeight;

  // viewport scrolling
  void scroll(unrestricted double x, unrestricted double y);
  void scroll(optional ScrollToOptions options = {});
  void scrollTo(unrestricted double x, unrestricted double y);
  void scrollTo(optional ScrollToOptions options = {});
  void scrollBy(unrestricted double x, unrestricted double y);
  void scrollBy(optional ScrollToOptions options = {});
  // mozScrollSnap is used by chrome to perform scroll snapping after the
  // user performs actions that may affect scroll position
  // mozScrollSnap is deprecated, to be replaced by a web accessible API, such
  // as an extension to the ScrollOptions dictionary.  See bug 1137937.
  [ChromeOnly] void mozScrollSnap();
  // The four properties below are double per spec at the moment, but whether
  // that will continue is unclear.
  [Replaceable, Throws] readonly attribute double scrollX;
  [Replaceable, Throws] readonly attribute double pageXOffset;
  [Replaceable, Throws] readonly attribute double scrollY;
  [Replaceable, Throws] readonly attribute double pageYOffset;

  // Aliases for screenX / screenY.
  [Replaceable, Throws, NeedsCallerType] readonly attribute double screenLeft;
  [Replaceable, Throws, NeedsCallerType] readonly attribute double screenTop;

  // client
  // These are writable because we allow chrome to write them.  And they need
  // to use 'any' as the type, because non-chrome writing them needs to act
  // like a [Replaceable] attribute would, which needs the original JS value.
  //[Replaceable, Throws] readonly attribute double screenX;
  //[Replaceable, Throws] readonly attribute double screenY;
  //[Replaceable, Throws] readonly attribute double outerWidth;
  //[Replaceable, Throws] readonly attribute double outerHeight;
  [Throws, NeedsCallerType] attribute any screenX;
  [Throws, NeedsCallerType] attribute any screenY;
  [Throws, NeedsCallerType] attribute any outerWidth;
  [Throws, NeedsCallerType] attribute any outerHeight;
};

// https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/RequestAnimationFrame/Overview.html
partial interface Window {
  [Throws] long requestAnimationFrame(FrameRequestCallback callback);
  [Throws] void cancelAnimationFrame(long handle);
};
callback FrameRequestCallback = void (DOMHighResTimeStamp time);

// https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html
partial interface Window {
  [Replaceable, Pure, StoreInSlot] readonly attribute Performance? performance;
};

// https://dvcs.w3.org/hg/webcrypto-api/raw-file/tip/spec/Overview.html
Window includes GlobalCrypto;

// https://fidoalliance.org/specifications/download/
Window includes GlobalU2F;

#ifdef MOZ_WEBSPEECH
// http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
interface mixin SpeechSynthesisGetter {
  [Throws, Pref="media.webspeech.synth.enabled"] readonly attribute SpeechSynthesis speechSynthesis;
};

Window includes SpeechSynthesisGetter;
#endif

// Mozilla-specific stuff
partial interface Window {
  //[NewObject, Throws] CSSStyleDeclaration getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");
  [NewObject, Throws] CSSStyleDeclaration? getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");

  // Mozilla extensions
  /**
   * Method for scrolling this window by a number of lines.
   */
  void                      scrollByLines(long numLines, optional ScrollOptions options = {});

  /**
   * Method for scrolling this window by a number of pages.
   */
  void                      scrollByPages(long numPages, optional ScrollOptions options = {});

  /**
   * Method for sizing this window to the content in the window.
   */
  [Throws, NeedsCallerType] void sizeToContent();

  // XXX Shouldn't this be in nsIDOMChromeWindow?
  [ChromeOnly, Replaceable, Throws] readonly attribute XULControllers controllers;

  [ChromeOnly, Throws] readonly attribute Element? realFrameElement;

  [ChromeOnly] readonly attribute nsIDocShell? docShell;

  [Throws, NeedsCallerType]
  readonly attribute float mozInnerScreenX;
  [Throws, NeedsCallerType]
  readonly attribute float mozInnerScreenY;
  [Replaceable, Throws, NeedsCallerType]
  readonly attribute double devicePixelRatio;

  /* The maximum offset that the window can be scrolled to
     (i.e., the document width/height minus the scrollport width/height) */
  [ChromeOnly, Throws]  readonly attribute long   scrollMinX;
  [ChromeOnly, Throws]  readonly attribute long   scrollMinY;
  [Replaceable, Throws] readonly attribute long   scrollMaxX;
  [Replaceable, Throws] readonly attribute long   scrollMaxY;

  [Throws] attribute boolean fullScreen;

  // XXX Should this be in nsIDOMChromeWindow?
  void                      updateCommands(DOMString action,
                                           optional Selection? sel = null,
                                           optional short reason = 0);

  /* Find in page.
   * @param str: the search pattern
   * @param caseSensitive: is the search caseSensitive
   * @param backwards: should we search backwards
   * @param wrapAround: should we wrap the search
   * @param wholeWord: should we search only for whole words
   * @param searchInFrames: should we search through all frames
   * @param showDialog: should we show the Find dialog
   */
  [Throws] boolean    find(optional DOMString str = "",
                           optional boolean caseSensitive = false,
                           optional boolean backwards = false,
                           optional boolean wrapAround = false,
                           optional boolean wholeWord = false,
                           optional boolean searchInFrames = false,
                           optional boolean showDialog = false);

  /**
   * Returns the number of times this document for this window has
   * been painted to the screen.
   *
   * If you need this for tests use nsIDOMWindowUtils.paintCount instead.
   */
  [Throws, Pref="dom.mozPaintCount.enabled"] readonly attribute unsigned long long mozPaintCount;

           attribute EventHandler ondevicemotion;
           attribute EventHandler ondeviceorientation;
           attribute EventHandler onabsolutedeviceorientation;
           attribute EventHandler ondeviceproximity;
           attribute EventHandler onuserproximity;
           attribute EventHandler ondevicelight;

  void                      dump(DOMString str);

  /**
   * This method is here for backwards compatibility with 4.x only,
   * its implementation is a no-op
   */
  void                      setResizable(boolean resizable);

  /**
   * This is the scriptable version of
   * nsPIDOMWindow::OpenDialog() that takes 3 optional
   * arguments, plus any additional arguments are passed on as
   * arguments on the dialog's window object (window.arguments).
   */
  [Throws, ChromeOnly] WindowProxy? openDialog(optional DOMString url = "",
                                               optional DOMString name = "",
                                               optional DOMString options = "",
                                               any... extraArguments);

  [
#ifdef NIGHTLY_BUILD
   ChromeOnly,
#endif
   NonEnumerable, Replaceable, Throws, NeedsCallerType]
  readonly attribute object? content;

  [Throws, ChromeOnly] any getInterface(any iid);

  /**
   * Same as nsIDOMWindow.windowRoot, useful for event listener targeting.
   */
  [ChromeOnly, Throws]
  readonly attribute WindowRoot? windowRoot;

  /**
   * ChromeOnly method to determine if a particular window should see console
   * reports from service workers of the given scope.
   */
  [ChromeOnly]
  boolean shouldReportForServiceWorkerScope(USVString aScope);

  /**
   * InstallTrigger is used for extension installs.  Ideally it would
   * be something like a WebIDL namespace, but we don't support
   * JS-implemented static things yet.  See bug 863952.
   */
  [Replaceable]
  readonly attribute InstallTriggerImpl? InstallTrigger;

  /**
   * Get the nsIDOMWindowUtils for this window.
   */
  [Constant, Throws, ChromeOnly]
  readonly attribute nsIDOMWindowUtils windowUtils;

  [ChromeOnly]
  readonly attribute boolean hasOpenerForInitialContentBrowser;

  [Pure, ChromeOnly]
  readonly attribute WindowGlobalChild? windowGlobalChild;
};

Window includes TouchEventHandlers;

Window includes OnErrorEventHandlerForWindow;

#if defined(MOZ_WIDGET_ANDROID)
// https://compat.spec.whatwg.org/#windoworientation-interface
partial interface Window {
  [NeedsCallerType]
  readonly attribute short orientation;
           attribute EventHandler onorientationchange;
};
#endif

#ifdef HAVE_SIDEBAR
// Mozilla extension
partial interface Window {
  [Replaceable, Throws, UseCounter, Pref="dom.sidebar.enabled"]
  readonly attribute (External or WindowProxy) sidebar;
};
#endif

[MOZ_CAN_RUN_SCRIPT_BOUNDARY]
callback PromiseDocumentFlushedCallback = any ();

// Mozilla extensions for Chrome windows.
partial interface Window {
  // The STATE_* constants need to match the corresponding enum in nsGlobalWindow.cpp.
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  const unsigned short STATE_MAXIMIZED = 1;
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  const unsigned short STATE_MINIMIZED = 2;
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  const unsigned short STATE_NORMAL = 3;
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  const unsigned short STATE_FULLSCREEN = 4;

  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  readonly attribute unsigned short windowState;

  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  readonly attribute boolean isFullyOccluded;

  /**
   * browserDOMWindow provides access to yet another layer of
   * utility functions implemented by chrome script. It will be null
   * for DOMWindows not corresponding to browsers.
   */
  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
           attribute nsIBrowserDOMWindow? browserDOMWindow;

  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      getAttention();

  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      getAttentionWithCycleCount(long aCycleCount);

  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      setCursor(UTF8String cursor);

  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      maximize();
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      minimize();
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      restore();
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  DOMString                 getWorkspaceID();
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void                      moveToWorkspace(DOMString workspaceID);

  /**
   * Notify a default button is loaded on a dialog or a wizard.
   * defaultButton is the default button.
   */
  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  void notifyDefaultButtonLoaded(Element defaultButton);

  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  readonly attribute ChromeMessageBroadcaster messageManager;

  /**
   * Returns the message manager identified by the given group name that
   * manages all frame loaders belonging to that group.
   */
  [Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  ChromeMessageBroadcaster getGroupMessageManager(DOMString aGroup);

  /**
   * Calls the given function as soon as a style or layout flush for the
   * top-level document is not necessary, and returns a Promise which
   * resolves to the callback's return value after it executes.
   *
   * In the event that the window goes away before a flush can occur, the
   * callback will still be called and the Promise resolved as the window
   * tears itself down.
   *
   * The callback _must not modify the DOM for any window in any way_. If it
   * does, after finishing executing, the Promise returned by
   * promiseDocumentFlushed will reject with
   * NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR.
   *
   * Note that the callback can be called either synchronously or asynchronously
   * depending on whether or not flushes are pending:
   *
   *   The callback will be called synchronously when calling
   *   promiseDocumentFlushed when NO flushes are already pending. This is
   *   to ensure that no script has a chance to dirty the DOM before the callback
   *   is called.
   *
   *   The callback will be called asynchronously if a flush is pending.
   *
   * The expected execution order is that all pending callbacks will
   * be fired first (and in the order that they were queued) and then the
   * Promise resolution handlers will all be invoked later on during the
   * next microtask checkpoint.
   *
   * Using window.top.promiseDocumentFlushed in combination with a callback
   * that is querying items in a window that might be swapped out via
   * nsFrameLoader::SwapWithOtherLoader is highly discouraged. For example:
   *
   *   let result = await window.top.promiseDocumentFlushed(() => {
   *     return window.document.body.getBoundingClientRect();
   *   });
   *
   *   If "window" might get swapped out via nsFrameLoader::SwapWithOtherLoader
   *   at any time, then the callback might get called when the new host window
   *   will still incur layout flushes, since it's only the original host window
   *   that's being monitored via window.top.promiseDocumentFlushed.
   *
   *   See bug 1519407 for further details.
   *
   * promiseDocumentFlushed does not support re-entrancy - so calling it from
   * within a promiseDocumentFlushed callback will result in the inner call
   * throwing an NS_ERROR_FAILURE exception, and the outer Promise rejecting
   * with that exception.
   *
   * The callback function *must not make any changes which would require
   * a style or layout flush*.
   *
   * Also throws NS_ERROR_FAILURE if the window is not in a state where flushes
   * can be waited for (for example, the PresShell has not yet been created).
   *
   * @param {function} callback
   * @returns {Promise}
   */
  [Throws, Func="nsGlobalWindowInner::IsPrivilegedChromeWindow"]
  Promise<any> promiseDocumentFlushed(PromiseDocumentFlushedCallback callback);

  [ChromeOnly]
  readonly attribute boolean isChromeWindow;
};

partial interface Window {
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplayconnect;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplaydisconnect;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplayactivate;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplaydeactivate;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplaypresentchange;
};

// https://drafts.css-houdini.org/css-paint-api-1/#dom-window-paintworklet
partial interface Window {
    [Pref="dom.paintWorklet.enabled", Throws]
    readonly attribute Worklet paintWorklet;
};

Window includes WindowOrWorkerGlobalScope;

partial interface Window {
  [Throws, Func="nsGlobalWindowInner::IsRequestIdleCallbackEnabled"]
  unsigned long requestIdleCallback(IdleRequestCallback callback,
                                    optional IdleRequestOptions options = {});
  [Func="nsGlobalWindowInner::IsRequestIdleCallbackEnabled"]
  void          cancelIdleCallback(unsigned long handle);
};

dictionary IdleRequestOptions {
  unsigned long timeout;
};

callback IdleRequestCallback = void (IdleDeadline deadline);

partial interface Window {
  /**
   * Returns a list of locales that the internationalization components
   * should be localized to.
   *
   * The function name refers to Regional Preferences which can be either
   * fetched from the internal internationalization database (CLDR), or
   * from the host environment.
   *
   * The result is a sorted list of valid locale IDs and it should be
   * used for all APIs that accept list of locales, like ECMA402 and L10n APIs.
   *
   * This API always returns at least one locale.
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   */
  [Func="IsChromeOrUAWidget"]
  sequence<DOMString> getRegionalPrefsLocales();

  /**
   * Returns a list of locales that the web content would know from the user.
   *
   * One of the fingerprinting technique is to recognize users from their locales
   * exposed to web content. For those components that would be fingerprintable
   * from the locale should call this API instead of |getRegionalPrefsLocales()|.
   *
   * If the pref is set to spoof locale setting, this function will return the
   * spoofed locale, otherwise it returns what |getRegionalPrefsLocales()| returns.
   *
   * This API always returns at least one locale.
   *
   * Example: ["en-US"]
   */
  [Func="IsChromeOrUAWidget"]
  sequence<DOMString> getWebExposedLocales();

  /**
   * Getter funcion for IntlUtils, which provides helper functions for
   * localization.
   */
  [Throws, Func="IsChromeOrUAWidget"]
  readonly attribute IntlUtils intlUtils;
};

partial interface Window {
  [SameObject, Pref="dom.visualviewport.enabled", Replaceable]
  readonly attribute VisualViewport visualViewport;

};

dictionary WindowPostMessageOptions : PostMessageOptions {
  USVString targetOrigin = "/";
};
