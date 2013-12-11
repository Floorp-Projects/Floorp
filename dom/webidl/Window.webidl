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
 */

interface ApplicationCache;
interface MediaQueryList;
interface MozFrameRequestCallback;
interface nsIDOMCrypto;
interface Pkcs11;
typedef any Transferable;

// http://www.whatwg.org/specs/web-apps/current-work/
[Global]
/*sealed*/ interface Window : EventTarget {
  // the current browsing context
  [Unforgeable, Throws,
   CrossOriginReadable] readonly attribute WindowProxy window;
  [Replaceable, Throws,
   CrossOriginReadable] readonly attribute WindowProxy self;
  //[Unforgeable] readonly attribute Document? document;
  [Throws] attribute DOMString name; 
  [PutForwards=href, Unforgeable, Throws,
   CrossOriginReadable, CrossOriginWritable] readonly attribute Location? location;
  [Throws] readonly attribute History history;
  [Replaceable, Throws] readonly attribute BarProp locationbar;
  [Replaceable, Throws] readonly attribute BarProp menubar;
  [Replaceable, Throws] readonly attribute BarProp personalbar;
  [Replaceable, Throws] readonly attribute BarProp scrollbars;
  [Replaceable, Throws] readonly attribute BarProp statusbar;
  [Replaceable, Throws] readonly attribute BarProp toolbar;
  [Throws] attribute DOMString status;
  [Throws, CrossOriginCallable] void close();
  [Throws, CrossOriginReadable] readonly attribute boolean closed;
  [Throws] void stop();
  [Throws, CrossOriginCallable] void focus();
  [Throws, CrossOriginCallable] void blur();

  // other browsing contexts
  [Replaceable, Throws, CrossOriginReadable] readonly attribute WindowProxy frames;
  [Replaceable, CrossOriginReadable] readonly attribute unsigned long length;
  [Unforgeable, Throws, CrossOriginReadable] readonly attribute WindowProxy top;
  [Throws, CrossOriginReadable] attribute WindowProxy? opener;
  //[Throws] readonly attribute WindowProxy parent;
  [Replaceable, Throws, CrossOriginReadable] readonly attribute WindowProxy? parent;
  [Throws] readonly attribute Element? frameElement;
  //[Throws] WindowProxy open(optional DOMString url = "about:blank", optional DOMString target = "_blank", optional DOMString features = "", optional boolean replace = false);
  [Throws] WindowProxy? open(optional DOMString url = "", optional DOMString target = "", optional DOMString features = "");
  // We think the indexed getter is a bug in the spec, it actually needs to live
  // on the WindowProxy
  //getter WindowProxy (unsigned long index);
  //getter object (DOMString name);

  // the user agent
  [Throws] readonly attribute Navigator navigator; 
  //(Not implemented)readonly attribute External external;
  [Throws] readonly attribute ApplicationCache applicationCache;

  // user prompts
  [Throws] void alert(optional DOMString message = "");
  [Throws] boolean confirm(optional DOMString message = "");
  [Throws] DOMString? prompt(optional DOMString message = "", optional DOMString default = "");
  [Throws] void print();
  //[Throws] any showModalDialog(DOMString url, optional any argument);
  [Throws] any showModalDialog(DOMString url, optional any argument, optional DOMString options = "");

  [Throws, CrossOriginCallable] void postMessage(any message, DOMString targetOrigin, optional sequence<Transferable> transfer);

  // also has obsolete members
};
Window implements GlobalEventHandlers;
Window implements WindowEventHandlers;

// http://www.whatwg.org/specs/web-apps/current-work/
[NoInterfaceObject]
interface WindowTimers {
  [Throws] long setTimeout(Function handler, optional long timeout = 0, any... arguments);
  [Throws] long setTimeout(DOMString handler, optional long timeout = 0);
  [Throws] void clearTimeout(long handle);
  [Throws] long setInterval(Function handler, optional long timeout, any... arguments);
  [Throws] long setInterval(DOMString handler, optional long timeout);
  [Throws] void clearInterval(long handle);
};
Window implements WindowTimers;

// http://www.whatwg.org/specs/web-apps/current-work/
[NoInterfaceObject]
interface WindowBase64 {
  [Throws] DOMString btoa(DOMString btoa);
  [Throws] DOMString atob(DOMString atob);
};
Window implements WindowBase64;

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
  void captureEvents(long dummy);
  void releaseEvents(long dummy);
};

// https://dvcs.w3.org/hg/editing/raw-file/tip/editing.html
partial interface Window {
  //[Throws] Selection getSelection();
  [Throws] Selection? getSelection();
};

// https://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html
Window implements IDBEnvironment;

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

partial interface Window {
  //[Throws] MediaQueryList matchMedia(DOMString query);
  [Throws] MediaQueryList? matchMedia(DOMString query);
  //[SameObject]
  [Throws] readonly attribute Screen screen;

  // browsing context
  //[Throws] void moveTo(double x, double y);
  //[Throws] void moveBy(double x, double y);
  //[Throws] void resizeTo(double x, double y);
  //[Throws] void resizeBy(double x, double y);
  [Throws] void moveTo(long x, long y);
  [Throws] void moveBy(long x, long y);
  [Throws] void resizeTo(long x, long y);
  [Throws] void resizeBy(long x, long y);

  // viewport
  //[Throws] readonly attribute double innerWidth;
  //[Throws] readonly attribute double innerHeight;
  [Throws] attribute long innerWidth;
  [Throws] attribute long innerHeight;

  // viewport scrolling
  //[Throws] readonly attribute double scrollX;
  //[Throws] readonly attribute double pageXOffset;
  //[Throws] readonly attribute double scrollY;
  //[Throws] readonly attribute double pageYOffset;
  //void scroll(double x, double y, optional ScrollOptions options);
  //void scrollTo(double x, double y, optional ScrollOptions options);
  //void scrollBy(double x, double y, optional ScrollOptions options);
  [Replaceable, Throws] readonly attribute long scrollX;
  [Throws] readonly attribute long pageXOffset;
  [Replaceable, Throws] readonly attribute long scrollY;
  [Throws] readonly attribute long pageYOffset;
  void scroll(long x, long y);
  void scrollTo(long x, long y);
  void scrollBy(long x, long y);

  // client
  //[Throws] readonly attribute double screenX;
  //[Throws] readonly attribute double screenY;
  //[Throws] readonly attribute double outerWidth;
  //[Throws] readonly attribute double outerHeight;
  [Throws] attribute long screenX;
  [Throws] attribute long screenY;
  [Throws] attribute long outerWidth;
  [Throws] attribute long outerHeight;
};

// https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/RequestAnimationFrame/Overview.html
partial interface Window {
  [Throws] long requestAnimationFrame(FrameRequestCallback callback);
  [Throws] void cancelAnimationFrame(long handle);
};
callback FrameRequestCallback = void (DOMHighResTimeStamp time);

// https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html
partial interface Window {
  [Replaceable, Throws] readonly attribute Performance? performance;
};

// https://dvcs.w3.org/hg/webcrypto-api/raw-file/tip/spec/Overview.html
partial interface Window {
  //[Throws] readonly attribute Crypto crypto;
  [Throws] readonly attribute nsIDOMCrypto crypto;
};

#ifdef MOZ_WEBSPEECH
// http://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html
[NoInterfaceObject]
interface SpeechSynthesisGetter {
  [Throws, Pref="media.webspeech.synth.enabled"] readonly attribute SpeechSynthesis speechSynthesis;
};

Window implements SpeechSynthesisGetter;
#endif

// Mozilla-specific stuff
partial interface Window {
  //[NewObject, Throws] CSSStyleDeclaration getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");
  [NewObject, Throws] CSSStyleDeclaration? getDefaultComputedStyle(Element elt, optional DOMString pseudoElt = "");

  [Throws] long mozRequestAnimationFrame(MozFrameRequestCallback aCallback);

  /**
   * Cancel a refresh callback.
   */
  [Throws] void mozCancelAnimationFrame(long aHandle);
  // Backwards-compat shim for now to make Google maps work
  [Throws] void mozCancelRequestAnimationFrame(long aHandle);

  /**
   * The current animation start time in milliseconds since the epoch.
   */
  [Throws] readonly attribute long long mozAnimationStartTime;

  // Mozilla extensions
  /**
   * Method for scrolling this window by a number of lines.
   */
  void                      scrollByLines(long numLines);

  /**
   * Method for scrolling this window by a number of pages.
   */
  void                      scrollByPages(long numPages);

  /**
   * Method for sizing this window to the content in the window.
   */
  [Throws] void             sizeToContent();

  readonly attribute Pkcs11?                      pkcs11;

  // XXX Shouldn't this be in nsIDOMChromeWindow?
  [Replaceable, Throws] readonly attribute MozControllers controllers;

  [Throws] readonly attribute float               mozInnerScreenX;
  [Throws] readonly attribute float               mozInnerScreenY;
  [Throws] readonly attribute float               devicePixelRatio;

  /* The maximum offset that the window can be scrolled to
     (i.e., the document width/height minus the scrollport width/height) */
  [Replaceable, Throws] readonly attribute long   scrollMaxX;
  [Replaceable, Throws] readonly attribute long   scrollMaxY;

  [Throws] attribute boolean                            fullScreen;

  [Throws] void             back();
  [Throws] void             forward();
  [Throws] void             home();

  // XXX Should this be in nsIDOMChromeWindow?
  void                      updateCommands(DOMString action);

  /* Find in page.
   * @param str: the search pattern
   * @param caseSensitive: is the search caseSensitive
   * @param backwards: should we search backwards
   * @param wrapAround: should we wrap the search
   * @param wholeWord: should we search only for whole words
   * @param searchInFrames: should we search through all frames
   * @param showDialog: should we show the Find dialog
   */
  [Throws] boolean          find(optional DOMString str = "",
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

           attribute EventHandler ondevicemotion;
           attribute EventHandler ondeviceorientation;
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
  [Throws, ChromeOnly] WindowProxy? openDialog(optional DOMString url = "",
                                               optional DOMString name = "",
                                               optional DOMString options = "",
                                               any... extraArguments);

  [Replaceable, Throws] readonly attribute object? content;

  [ChromeOnly, Throws] readonly attribute object? __content;
};

Window implements TouchEventHandlers;

Window implements OnErrorEventHandlerForWindow;
