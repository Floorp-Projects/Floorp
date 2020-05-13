/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface LoadContext;
interface RemoteTab;
interface URI;
interface nsIDocShell;
interface nsIPrintSettings;
interface nsIWebBrowserPersistDocumentReceiver;
interface nsIWebProgressListener;

[ChromeOnly,
 Exposed=Window]
interface FrameLoader {
  /**
   * Get the docshell from the frame loader.
   */
  [GetterThrows]
  readonly attribute nsIDocShell? docShell;

  /**
   * Get this frame loader's RemoteTab, if it has a remote frame.  Otherwise,
   * returns null.
   */
  readonly attribute RemoteTab? remoteTab;

  /**
   * Get an nsILoadContext for the top-level docshell. For remote
   * frames, a shim is returned that contains private browsing and app
   * information.
   */
  readonly attribute LoadContext loadContext;

  /**
   * Get the root BrowsingContext within the frame.
   * This may be null immediately after creating a remote frame.
   */
  readonly attribute BrowsingContext? browsingContext;

  /**
   * Find out whether the loader's frame is at too great a depth in
   * the frame tree.  This can be used to decide what operations may
   * or may not be allowed on the loader's docshell.
   */
  [Pure]
  readonly attribute boolean depthTooGreat;

  /**
   * Find out whether the loader's frame is a remote frame.
   */
  readonly attribute boolean isRemoteFrame;

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
  readonly attribute MessageSender? messageManager;

  /**
   * Request that the next time a remote layer transaction has been
   * received by the Compositor, a MozAfterRemoteFrame event be sent
   * to the window.
   */
  void requestNotifyAfterRemotePaint();

  /**
   * Force a remote browser to recompute its dimension and screen position.
   */
  [Throws]
  void requestUpdatePosition();

  /**
   * Force a TabStateFlush from native sessionStoreListeners.
   * Return true if the flush requires async ipc call.
   */
  boolean requestTabStateFlush(unsigned long aFlushId);

  /**
   * Force Epoch update in native sessionStoreListeners.
   */
  void requestEpochUpdate(unsigned long aEpoch);

  /**
   * Request a session history update in native sessionStoreListeners.
   */
  void requestSHistoryUpdate(boolean aImmediately);

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
   * The element which owns this frame loader.
   *
   * For example, if this is a frame loader for an <iframe>, this attribute
   * returns the iframe element.
   */
  [Pure]
  readonly attribute Element? ownerElement;


  /**
   * Cached childID of the ContentParent owning the RemoteTab in this frame
   * loader. This can be used to obtain the childID after the RemoteTab died.
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

/**
 * Interface for objects which represent a document that can be
 * serialized with nsIWebBrowserPersist.  This interface is
 * asynchronous because the actual document can be in another process
 * (e.g., if this object is a FrameLoader for an out-of-process
 * frame).
 *
 * @see nsIWebBrowserPersistDocumentReceiver
 * @see nsIWebBrowserPersistDocument
 * @see nsIWebBrowserPersist
 *
 * @param aContext
 *        The browsing context of the subframe we'd like to persist.
 *        If set to nullptr, WebBrowserPersistable will attempt to persist
 *        the top-level document. If the browsing context is for a subframe
 *        that is not held beneath the WebBrowserPersistable, aRecv's onError
 *        method will be called with NS_ERROR_NO_CONTENT.
 * @param aRecv
 *        The nsIWebBrowserPersistDocumentReceiver is a callback that
 *        will be fired once the document is ready for persisting.
 */
interface mixin WebBrowserPersistable
{
  [Throws]
  void startPersistence(BrowsingContext? aContext,
                        nsIWebBrowserPersistDocumentReceiver aRecv);
};

FrameLoader includes WebBrowserPersistable;
