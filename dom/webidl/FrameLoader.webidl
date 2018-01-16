/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface LoadContext;
interface TabParent;
interface URI;
interface nsIDocShell;
interface nsIMessageSender;
interface nsIPrintSettings;
interface nsIWebBrowserPersistDocumentReceiver;
interface nsIWebProgressListener;

[ChromeOnly]
interface FrameLoader {
  /**
   * Get the docshell from the frame loader.
   */
  [GetterThrows]
  readonly attribute nsIDocShell? docShell;

  /**
   * Get this frame loader's TabParent, if it has a remote frame.  Otherwise,
   * returns null.
   */
  readonly attribute TabParent? tabParent;

  /**
   * Get an nsILoadContext for the top-level docshell. For remote
   * frames, a shim is returned that contains private browsing and app
   * information.
   */
  readonly attribute LoadContext loadContext;

  /**
   * Start loading the frame. This method figures out what to load
   * from the owner content in the frame loader.
   */
  [Throws]
  void loadFrame(optional boolean originalSrc = false);

  /**
   * Loads the specified URI in this frame. Behaves identically to loadFrame,
   * except that this method allows specifying the URI to load.
   */
  [Throws]
  void loadURI(URI aURI, optional boolean originalSrc = false);

  /**
   * Adds a blocking promise for the current cross process navigation.
   * This method can only be called while the "BrowserWillChangeProcess" event
   * is being fired.
   */
  [Throws]
  void addProcessChangeBlockingPromise(Promise<any> aPromise);

  /**
   * Destroy the frame loader and everything inside it. This will
   * clear the weak owner content reference.
   */
  [Throws]
  void destroy();

  /**
   * Find out whether the loader's frame is at too great a depth in
   * the frame tree.  This can be used to decide what operations may
   * or may not be allowed on the loader's docshell.
   */
  [Pure]
  readonly attribute boolean depthTooGreat;

  /**
   * Activate remote frame.
   * Throws an exception with non-remote frames.
   */
  [Throws]
  void activateRemoteFrame();

  /**
   * Deactivate remote frame.
   * Throws an exception with non-remote frames.
   */
  [Throws]
  void deactivateRemoteFrame();

  /**
   * @see nsIDOMWindowUtils sendMouseEvent.
   */
  [Throws]
  void sendCrossProcessMouseEvent(DOMString aType,
                                  float aX,
                                  float aY,
                                  long aButton,
                                  long aClickCount,
                                  long aModifiers,
                                  optional boolean aIgnoreRootScrollFrame = false);

  /**
   * Activate event forwarding from client (remote frame) to parent.
   */
  [Throws]
  void activateFrameEvent(DOMString aType, boolean capture);

  // Note, when frameloaders are swapped, also messageManagers are swapped.
  readonly attribute nsIMessageSender? messageManager;

  /**
   * @see nsIDOMWindowUtils sendKeyEvent.
   */
  [Throws]
  void sendCrossProcessKeyEvent(DOMString aType,
                                long aKeyCode,
                                long aCharCode,
                                long aModifiers,
                                optional boolean aPreventDefault = false);

  /**
   * Request that the next time a remote layer transaction has been
   * received by the Compositor, a MozAfterRemoteFrame event be sent
   * to the window.
   */
  [Throws]
  void requestNotifyAfterRemotePaint();

  /**
   * Close the window through the ownerElement.
   */
  [Throws]
  void requestFrameLoaderClose();

  /**
   * Force a remote browser to recompute its dimension and screen position.
   */
  [Throws]
  void requestUpdatePosition();

  /**
   * Print the current document.
   *
   * @param aOuterWindowID the ID of the outer window to print
   * @param aPrintSettings optional print settings to use; printSilent can be
   *                       set to prevent prompting.
   * @param aProgressListener optional print progress listener.
   */
  [Throws]
  void print(unsigned long long aOuterWindowID,
             nsIPrintSettings aPrintSettings,
             optional nsIWebProgressListener? aProgressListener = null);

  /**
   * The default event mode automatically forwards the events
   * handled in EventStateManager::HandleCrossProcessEvent to
   * the child content process when these events are targeted to
   * the remote browser element.
   *
   * Used primarly for input events (mouse, keyboard)
   */
  const unsigned long EVENT_MODE_NORMAL_DISPATCH = 0x00000000;

  /**
   * With this event mode, it's the application's responsability to
   * convert and forward events to the content process
   */
  const unsigned long EVENT_MODE_DONT_FORWARD_TO_CHILD = 0x00000001;

  [Pure]
  attribute unsigned long eventMode;

  /**
   * If false, then the subdocument is not clipped to its CSS viewport, and the
   * subdocument's viewport scrollbar(s) are not rendered.
   * Defaults to true.
   */
  attribute boolean clipSubdocument;

  /**
   * If false, then the subdocument's scroll coordinates will not be clamped
   * to their scroll boundaries.
   * Defaults to true.
   */
  attribute boolean clampScrollPosition;

  /**
   * The element which owns this frame loader.
   *
   * For example, if this is a frame loader for an <iframe>, this attribute
   * returns the iframe element.
   */
  [Pure]
  readonly attribute Element? ownerElement;


  /**
   * Cached childID of the ContentParent owning the TabParent in this frame
   * loader. This can be used to obtain the childID after the TabParent died.
   */
  [Pure]
  readonly attribute unsigned long long childID;

  /**
   * Find out whether the owner content really is a mozbrowser. <xul:browser>
   * is not considered to be a mozbrowser frame.
   */
  [Pure]
  readonly attribute boolean ownerIsMozBrowserFrame;

  /**
   * The last known width of the frame. Reading this property will not trigger
   * a reflow, and therefore may not reflect the current state of things. It
   * should only be used in asynchronous APIs where values are not guaranteed
   * to be up-to-date when received.
   */
  [Pure]
  readonly attribute unsigned long lazyWidth;

  /**
   * The last known height of the frame. Reading this property will not trigger
   * a reflow, and therefore may not reflect the current state of things. It
   * should only be used in asynchronous APIs where values are not guaranteed
   * to be up-to-date when received.
   */
  [Pure]
  readonly attribute unsigned long lazyHeight;

  /**
   * Is `true` if the frameloader is dead (destroy has been called on it)
   */
  [Pure]
  readonly attribute boolean isDead;
};

[NoInterfaceObject]
interface WebBrowserPersistable
{
  [Throws]
  void startPersistence(unsigned long long aOuterWindowID,
                        nsIWebBrowserPersistDocumentReceiver aRecv);
};

FrameLoader implements WebBrowserPersistable;
