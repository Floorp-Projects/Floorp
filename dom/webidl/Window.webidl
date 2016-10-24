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
 */

interface ApplicationCache;
interface IID;
interface nsIBrowserDOMWindow;
interface nsIMessageBroadcaster;
interface nsIDOMCrypto;
typedef any Transferable;

// http://www.whatwg.org/specs/web-apps/current-work/
[PrimaryGlobal, LegacyUnenumerableNamedProperties, NeedResolve]
/*sealed*/ interface Window : EventTarget {
  // the current browsing context
  [Unforgeable, Constant, StoreInSlot,
   CrossOriginReadable] readonly attribute Window window;
  [Replaceable, Constant, StoreInSlot,
   CrossOriginReadable] readonly attribute Window self;
  [Unforgeable, StoreInSlot, Pure] readonly attribute Document? document;
  [Throws] attribute DOMString name;
  [PutForwards=href, Unforgeable, Throws,
   CrossOriginReadable, CrossOriginWritable] readonly attribute Location? location;
  [Throws] readonly attribute History history;
  [Func="CustomElementRegistry::IsCustomElementEnabled"]
  readonly attribute CustomElementRegistry customElements;
  [Replaceable, Throws] readonly attribute BarProp locationbar;
  [Replaceable, Throws] readonly attribute BarProp menubar;
  [Replaceable, Throws] readonly attribute BarProp personalbar;
  [Replaceable, Throws] readonly attribute BarProp scrollbars;
  [Replaceable, Throws] readonly attribute BarProp statusbar;
  [Replaceable, Throws] readonly attribute BarProp toolbar;
  [Throws] attribute DOMString status;
  [Throws, CrossOriginCallable, UnsafeInPrerendering] void close();
  [Throws, CrossOriginReadable] readonly attribute boolean closed;
  [Throws] void stop();
  [Throws, CrossOriginCallable, UnsafeInPrerendering] void focus();
  [Throws, CrossOriginCallable] void blur();

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
  [Throws, UnsafeInPrerendering] WindowProxy? open(optional DOMString url = "", optional DOMString target = "", [TreatNullAs=EmptyString] optional DOMString features = "");
  getter object (DOMString name);

  // the user agent
  [Throws] readonly attribute Navigator navigator;
#ifdef HAVE_SIDEBAR
  [Replaceable, Throws] readonly attribute External external;
#endif
  [Throws, Pref="browser.cache.offline.enable"] readonly attribute ApplicationCache applicationCache;

  // user prompts
  [Throws, UnsafeInPrerendering, NeedsSubjectPrincipal] void alert();
  [Throws, UnsafeInPrerendering, NeedsSubjectPrincipal] void alert(DOMString message);
  [Throws, UnsafeInPrerendering, NeedsSubjectPrincipal] boolean confirm(optional DOMString message = "");
  [Throws, UnsafeInPrerendering, NeedsSubjectPrincipal] DOMString? prompt(optional DOMString message = "", optional DOMString default = "");
  [Throws, UnsafeInPrerendering] void print();
  //[Throws] any showModalDialog(DOMString url, optional any argument);
  [Throws, Func="nsGlobalWindow::IsShowModalDialogEnabled", UnsafeInPrerendering, NeedsSubjectPrincipal]
  any showModalDialog(DOMString url, optional any argument, optional DOMString options = "");

  [Throws, CrossOriginCallable, NeedsSubjectPrincipal]
  void postMessage(any message, DOMString targetOrigin, optional sequence<Transferable> transfer);

  // also has obsolete members
};
Window implements GlobalEventHandlers;
Window implements WindowEventHandlers;

// https://www.w3.org/TR/appmanifest/#onappinstalled-attribute
partial interface Window {
  [Pref="dom.manifest.onappinstalled"]
  attribute EventHandler onappinstalled;
};

// http://www.whatwg.org/specs/web-apps/current-work/
[NoInterfaceObject]
interface WindowSessionStorage {
  //[Throws] readonly attribute Storage sessionStorage;
  [Throws] readonly attribute Storage? sessionStorage;
};
Window implements WindowSessionStorage;

// http://www.whatwg.org/specs/web-apps/current-work/
[NoInterfaceObject]
interface WindowLocalStorage {
  [Throws] readonly attribute Storage? localStorage;
};
Window implements WindowLocalStorage;

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
  //[Throws,NewObject] MediaQueryList matchMedia(DOMString query);
  [Throws,NewObject] MediaQueryList? matchMedia(DOMString query);
  // Per spec, screen is SameObject, but we don't actually guarantee that given
  // nsGlobalWindow::Cleanup.  :(
  //[SameObject, Replaceable, Throws] readonly attribute Screen screen;
  [Replaceable, Throws] readonly attribute Screen screen;

  // browsing context
  //[Throws] void moveTo(double x, double y);
  //[Throws] void moveBy(double x, double y);
  //[Throws] void resizeTo(double x, double y);
  //[Throws] void resizeBy(double x, double y);
  [Throws, UnsafeInPrerendering] void moveTo(long x, long y);
  [Throws, UnsafeInPrerendering] void moveBy(long x, long y);
  [Throws, UnsafeInPrerendering] void resizeTo(long x, long y);
  [Throws, UnsafeInPrerendering] void resizeBy(long x, long y);

  // viewport
  // These are writable because we allow chrome to write them.  And they need
  // to use 'any' as the type, because non-chrome writing them needs to act
  // like a [Replaceable] attribute would, which needs the original JS value.
  //[Replaceable, Throws] readonly attribute double innerWidth;
  //[Replaceable, Throws] readonly attribute double innerHeight;
  [Throws] attribute any innerWidth;
  [Throws] attribute any innerHeight;

  // viewport scrolling
  void scroll(unrestricted double x, unrestricted double y);
  void scroll(optional ScrollToOptions options);
  void scrollTo(unrestricted double x, unrestricted double y);
  void scrollTo(optional ScrollToOptions options);
  void scrollBy(unrestricted double x, unrestricted double y);
  void scrollBy(optional ScrollToOptions options);
  // mozScrollSnap is used by chrome to perform scroll snapping after the
  // user performs actions that may affect scroll position
  // mozScrollSnap is deprecated, to be replaced by a web accessible API, such
  // as an extension to the ScrollOptions dictionary.  See bug 1137937.
  [ChromeOnly] void mozScrollSnap();
  // The four properties below are double per spec at the moment, but whether
  // that will continue is unclear.
  //[Replaceable, Throws] readonly attribute double scrollX;
  //[Replaceable, Throws] readonly attribute double pageXOffset;
  //[Replaceable, Throws] readonly attribute double scrollY;
  //[Replaceable, Throws] readonly attribute double pageYOffset;
  [Replaceable, Throws] readonly attribute long scrollX;
  [Replaceable, Throws] readonly attribute long pageXOffset;
  [Replaceable, Throws] readonly attribute long scrollY;
  [Replaceable, Throws] readonly attribute long pageYOffset;

  // client
  // These are writable because we allow chrome to write them.  And they need
  // to use 'any' as the type, because non-chrome writing them needs to act
  // like a [Replaceable] attribute would, which needs the original JS value.
  //[Replaceable, Throws] readonly attribute double screenX;
  //[Replaceable, Throws] readonly attribute double screenY;
  //[Replaceable, Throws] readonly attribute double outerWidth;
  //[Replaceable, Throws] readonly attribute double outerHeight;
  [Throws] attribute any screenX;
  [Throws] attribute any screenY;
  [Throws] attribute any outerWidth;
  [Throws] attribute any outerHeight;
};

/**
 * Special function that gets the fill ratio from the compositor used for testing
 * and is an indicator that we're layerizing correctly.
 * This function will call the given callback current fill ratio for a
 * composited frame. We don't guarantee which frame fill ratios will be returned.
 */
partial interface Window {
  [ChromeOnly, Throws] void mozRequestOverfill(OverfillCallback callback);
};
callback OverfillCallback = void (unsigned long overfill);

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
Window implements GlobalCrypto;

// https://fidoalliance.org/specifications/download/
Window implements GlobalU2F;

#ifdef MOZ_WEBSPEECH
// http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
[NoInterfaceObject]
interface SpeechSynthesisGetter {
  [Throws, Pref="media.webspeech.synth.enabled"] readonly attribute SpeechSynthesis speechSynthesis;
};

Window implements SpeechSynthesisGetter;
#endif

// http://www.whatwg.org/specs/web-apps/current-work/
[NoInterfaceObject]
interface WindowModal {
  [Throws, Func="nsGlobalWindow::IsModalContentWindow", NeedsSubjectPrincipal]
  readonly attribute any dialogArguments;

  [Throws, Func="nsGlobalWindow::IsModalContentWindow", NeedsSubjectPrincipal]
  attribute any returnValue;
};
Window implements WindowModal;

// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#self-caches
partial interface Window {
[Throws, Func="mozilla::dom::cache::CacheStorage::PrefEnabled", SameObject]
readonly attribute CacheStorage caches;
};

// Mozilla-specific stuff
partial interface Window {
  //[NewObject, Throws] CSSStyleDeclaration getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");
  [NewObject, Throws] CSSStyleDeclaration? getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");

  // Mozilla extensions
  /**
   * Method for scrolling this window by a number of lines.
   */
  void                      scrollByLines(long numLines, optional ScrollOptions options);

  /**
   * Method for scrolling this window by a number of pages.
   */
  void                      scrollByPages(long numPages, optional ScrollOptions options);

  /**
   * Method for sizing this window to the content in the window.
   */
  [Throws, UnsafeInPrerendering] void             sizeToContent();

  // XXX Shouldn't this be in nsIDOMChromeWindow?
  [ChromeOnly, Replaceable, Throws] readonly attribute MozControllers controllers;

  [ChromeOnly, Throws] readonly attribute Element? realFrameElement;

  [Throws] readonly attribute float               mozInnerScreenX;
  [Throws] readonly attribute float               mozInnerScreenY;
  [Replaceable, Throws] readonly attribute float  devicePixelRatio;

  /* The maximum offset that the window can be scrolled to
     (i.e., the document width/height minus the scrollport width/height) */
  [ChromeOnly, Throws]  readonly attribute long   scrollMinX;
  [ChromeOnly, Throws]  readonly attribute long   scrollMinY;
  [Replaceable, Throws] readonly attribute long   scrollMaxX;
  [Replaceable, Throws] readonly attribute long   scrollMaxY;

  [Throws, UnsafeInPrerendering] attribute boolean            fullScreen;

  [Throws, ChromeOnly, UnsafeInPrerendering] void             back();
  [Throws, ChromeOnly, UnsafeInPrerendering] void             forward();
  [Throws, ChromeOnly, UnsafeInPrerendering] void             home();

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
  [Throws, UnsafeInPrerendering] boolean    find(optional DOMString str = "",
                                                 optional boolean caseSensitive = false,
                                                 optional boolean backwards = false,
                                                 optional boolean wrapAround = false,
                                                 optional boolean wholeWord = false,
                                                 optional boolean searchInFrames = false,
                                                 optional boolean showDialog = false);

  /**
   * Returns the number of times this document for this window has
   * been painted to the screen.
   */
  [Throws] readonly attribute unsigned long long mozPaintCount;

  /**
   * This property exists because static attributes don't yet work for
   * JS-implemented WebIDL (see bugs 1058606 and 863952). With this hack, we
   * can use `MozSelfSupport.something(...)`, which will continue to work
   * after we ditch this property and switch to static attributes. See
   */
  [ChromeOnly, Throws] readonly attribute MozSelfSupport MozSelfSupport;

  [Pure]
           attribute EventHandler onwheel;

           attribute EventHandler ondevicemotion;
           attribute EventHandler ondeviceorientation;
           attribute EventHandler onabsolutedeviceorientation;
           attribute EventHandler ondeviceproximity;
           attribute EventHandler onuserproximity;
           attribute EventHandler ondevicelight;

#ifdef MOZ_B2G
           attribute EventHandler onmoztimechange;
           attribute EventHandler onmoznetworkupload;
           attribute EventHandler onmoznetworkdownload;
#endif

  void                      dump(DOMString str);

  /**
   * This method is here for backwards compatibility with 4.x only,
   * its implementation is a no-op
   */
  void                      setResizable(boolean resizable);

  /**
   * This is the scriptable version of
   * nsIDOMWindow::openDialog() that takes 3 optional
   * arguments, plus any additional arguments are passed on as
   * arguments on the dialog's window object (window.arguments).
   */
  [Throws, ChromeOnly, UnsafeInPrerendering] WindowProxy? openDialog(optional DOMString url = "",
                                                                   optional DOMString name = "",
                                                                   optional DOMString options = "",
                                                                   any... extraArguments);

  [Replaceable, Throws] readonly attribute object? content;

  [ChromeOnly, Throws] readonly attribute object? __content;

  [Throws, ChromeOnly] any getInterface(IID iid);

  /**
   * Same as nsIDOMWindow.windowRoot, useful for event listener targeting.
   */
  [ChromeOnly, Throws]
  readonly attribute WindowRoot? windowRoot;
};

Window implements TouchEventHandlers;

Window implements OnErrorEventHandlerForWindow;

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
// https://compat.spec.whatwg.org/#windoworientation-interface
partial interface Window {
  readonly attribute short orientation;
           attribute EventHandler onorientationchange;
};
#endif

#ifdef HAVE_SIDEBAR
// Mozilla extension
partial interface Window {
  [Replaceable, Throws, UseCounter]
  readonly attribute (External or WindowProxy) sidebar;
};
#endif

[Func="IsChromeOrXBL"]
interface ChromeWindow {
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  const unsigned short STATE_MAXIMIZED = 1;
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  const unsigned short STATE_MINIMIZED = 2;
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  const unsigned short STATE_NORMAL = 3;
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  const unsigned short STATE_FULLSCREEN = 4;

  [Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  readonly attribute unsigned short windowState;

  /**
   * browserDOMWindow provides access to yet another layer of
   * utility functions implemented by chrome script. It will be null
   * for DOMWindows not corresponding to browsers.
   */
  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
           attribute nsIBrowserDOMWindow? browserDOMWindow;

  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  void                      getAttention();

  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  void                      getAttentionWithCycleCount(long aCycleCount);

  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  void                      setCursor(DOMString cursor);

  [Func="nsGlobalWindow::IsPrivilegedChromeWindow", UnsafeInPrerendering]
  void                      maximize();
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow", UnsafeInPrerendering]
  void                      minimize();
  [Func="nsGlobalWindow::IsPrivilegedChromeWindow", UnsafeInPrerendering]
  void                      restore();

  /**
   * Notify a default button is loaded on a dialog or a wizard.
   * defaultButton is the default button.
   */
  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  void notifyDefaultButtonLoaded(Element defaultButton);

  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  readonly attribute nsIMessageBroadcaster messageManager;

  /**
   * Returns the message manager identified by the given group name that
   * manages all frame loaders belonging to that group.
   */
  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  nsIMessageBroadcaster getGroupMessageManager(DOMString aGroup);

  /**
   * On some operating systems, we must allow the window manager to
   * handle window dragging. This function tells the window manager to
   * start dragging the window. This function will fail unless called
   * while the left mouse button is held down, callers must check this.
   *
   * The optional panel argument should be set when moving a panel.
   *
   * Throws NS_ERROR_NOT_IMPLEMENTED if the OS doesn't support this.
   */
  [Throws, Func="nsGlobalWindow::IsPrivilegedChromeWindow"]
  void beginWindowMove(Event mouseDownEvent, optional Element? panel = null);
};

partial interface Window {
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplayconnect;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplaydisconnect;
  [Pref="dom.vr.enabled"]
  attribute EventHandler onvrdisplaypresentchange;
};

Window implements ChromeWindow;
Window implements WindowOrWorkerGlobalScope;

partial interface Window {
  [Throws, Pref="dom.requestIdleCallback.enabled"]
  unsigned long requestIdleCallback(IdleRequestCallback callback,
                                    optional IdleRequestOptions options);
  [Pref="dom.requestIdleCallback.enabled"]
  void          cancelIdleCallback(unsigned long handle);
};

dictionary IdleRequestOptions {
  unsigned long timeout;
};

callback IdleRequestCallback = void (IdleDeadline deadline);
