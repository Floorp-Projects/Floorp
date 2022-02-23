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
   * Activate event forwarding from client (remote frame) to parent.
   */
  [Throws]
  void activateFrameEvent(DOMString aType, boolean capture);

  // Note, when frameloaders are swapped, also messageManagers are swapped.
  readonly attribute MessageSender? messageManager;

  /**
   * Force a remote browser to recompute its dimension and screen position.
   */
  [Throws]
  void requestUpdatePosition();

  /**
   * Force a TabStateFlush from native sessionStoreListeners.
   * Returns a promise that resolves when all session store data has been
   * flushed.
   */
  [Throws]
  Promise<void> requestTabStateFlush();

  /**
   * Force Epoch update in native sessionStoreListeners.
   */
  void requestEpochUpdate(unsigned long aEpoch);

  /**
   * Request a session history update in native sessionStoreListeners.
   */
  void requestSHistoryUpdate();

  /**
   * Creates a print preview document in this frame, or updates the existing
   * print preview document with new print settings.
   *
   * @param aPrintSettings The print settings to use to layout the print
   *   preview document.
   * @param aSourceBrowsingContext Optionally, the browsing context that
   *   contains the document from which the print preview is to be generated,
   *   which must be in the same process as the browsing context of the frame
   *   loader itself.
   *
   *   This should only be passed on the first call.  It should not be passed
   *   for any subsequent calls that are made to update the existing print
   *   preview document with a new print settings object.
   * @return A Promise that resolves with a PrintPreviewSuccessInfo on success.
   */
  [ChromeOnly, Throws]
  Promise<unsigned long> printPreview(nsIPrintSettings aPrintSettings,
                                      BrowsingContext? aSourceBrowsingContext);

  /**
   * Inform the print preview document that we're done with it.
   */
  [ChromeOnly]
  void exitPrintPreview();

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

enum PrintPreviewOrientation {
    "landscape",
    "portrait",
    "unspecified"
};

/**
 * Interface for the object that's used to resolve the Promise returned from
 * FrameLoader.printPreview() if that method successfully creates the print
 * preview document/successfully updates it with new settings.
 */
[GenerateConversionToJS]
dictionary PrintPreviewSuccessInfo {
  /**
   * The total number of sheets of paper required to print, taking into account
   * the provided nsIPrintSettings.  This takes into account page range
   * selection, the pages-per-sheet, whether duplex printing is enabled, etc.
   */
  unsigned long sheetCount = 0;

  /**
   * The total number of virtual pages, not taking into account page range
   * selection, the pages-per-sheet, whether duplex printing is enabled, etc.
   */
  unsigned long totalPageCount = 0;

  /**
   * Whether the preview is empty because of page range selection.
   */
  boolean isEmpty = false;

  /**
   * Whether the document or any subdocument has a selection that can be
   * printed.
   */
  boolean hasSelection = false;

  /**
   * Whether the previewed document has a selection itself.
   */
  boolean hasSelfSelection = false;

  /**
   * Specified orientation of the document, or "unspecified".
   */
  PrintPreviewOrientation orientation = "unspecified";
};

FrameLoader includes WebBrowserPersistable;
