/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIEventTarget;
interface Principal;

/**
 * Message managers provide a way for chrome-privileged JS code to
 * communicate with each other, even across process boundaries.
 *
 * Message managers are separated into "parent side" and "child side".
 * These don't always correspond to process boundaries, but can.  For
 * each child-side message manager, there is always exactly one
 * corresponding parent-side message manager that it sends messages
 * to.  However, for each parent-side message manager, there may be
 * either one or many child-side managers it can message.
 *
 * Message managers that always have exactly one "other side" are of
 * type MessageSender.  Parent-side message managers that have many
 * "other sides" are of type MessageBroadcaster.
 *
 * Child-side message managers can send synchronous messages to their
 * parent side, but not the other way around.
 *
 * There are two realms of message manager hierarchies.  One realm
 * approximately corresponds to DOM elements, the other corresponds to
 * process boundaries.
 *
 * Message managers corresponding to DOM elements
 * ==============================================
 *
 * In this realm of message managers, there are
 *  - "frame message managers" which correspond to frame elements
 *  - "window message managers" which correspond to top-level chrome
 *    windows
 *  - "group message managers" which correspond to named message
 *    managers with a specific window MM as the parent
 *  - the "global message manager", on the parent side.  See below.
 *
 * The DOM-realm message managers can communicate in the ways shown by
 * the following diagram.  The parent side and child side can
 * correspond to process boundaries, but don't always.
 *
 *  Parent side                         Child side
 * -------------                       ------------
 *  global MMg
 *   |
 *   +-->window MMw1
 *   |    |
 *   |    +-->frame MMp1_1<------------>frame MMc1_1
 *   |    |
 *   |    +-->frame MMp1_2<------------>frame MMc1_2
 *   |    |
 *   |    +-->group MMgr1
 *   |    |    |
 *   |    |    +-->frame MMp2_1<------->frame MMc2_1
 *   |    |    |
 *   |    |    +-->frame MMp2_2<------->frame MMc2_2
 *   |    |
 *   |    +-->group MMgr2
 *   |    |    ...
 *   |    |
 *   |    ...
 *   |
 *   +-->window MMw2
 *   ...
 *
 * For example: a message sent from MMc1_1, from the child side, is
 * sent only to MMp1_1 on the parent side.  However, note that all
 * message managers in the hierarchy above MMp1_1, in this diagram
 * MMw1 and MMg, will also notify their message listeners when the
 * message arrives.
 *
 * A message sent from MMc2_1 will be sent to MMp2_1 and also notify
 * all message managers in the hierarchy above that, including the
 * group message manager MMgr1.

 * For example: a message broadcast through the global MMg on the
 * parent side would be broadcast to MMw1, which would transitively
 * broadcast it to MMp1_1, MM1p_2.  The message would next be
 * broadcast to MMgr1, which would broadcast it to MMp2_1 and MMp2_2.
 * After that it would broadcast to MMgr2 and then to MMw2, and so
 * on down the hierarchy.
 *
 *   ***** PERFORMANCE AND SECURITY WARNING *****
 * Messages broadcast through the global MM and window or group MMs
 * can result in messages being dispatched across many OS processes,
 * and to many processes with different permissions.  Great care
 * should be taken when broadcasting.
 *
 * Interfaces
 * ----------
 *
 * The global MMg and window MMw's are message broadcasters implementing
 * MessageBroadcaster while the frame MMp's are simple message senders (MessageSender).
 * Their counterparts in the content processes are message senders implementing
 * ContentFrameMessageManager.
 *
 *                 MessageListenerManager
 *               /                        \
 * MessageSender                            MessageBroadcaster
 *       |
 * SyncMessageSender (content process/in-process only)
 *       |
 * ContentFrameMessageManager (content process/in-process only)
 *       |
 * nsIInProcessContentFrameMessageManager (in-process only)
 *
 *
 * Message managers in the chrome process also implement FrameScriptLoader.
 *
 *
 * Message managers corresponding to process boundaries
 * ====================================================
 *
 * The second realm of message managers is the "process message
 * managers".  With one exception, these always correspond to process
 * boundaries.  The picture looks like
 *
 *  Parent process                      Child processes
 * ----------------                    -----------------
 *  global (GPPMM)
 *   |
 *   +-->parent in-process PIPMM<-->child in-process CIPPMM
 *   |
 *   +-->parent (PPMM1)<------------------>child (CPMM1)
 *   |
 *   +-->parent (PPMM2)<------------------>child (CPMM2)
 *   ...
 *
 * Note, PIPMM and CIPPMM both run in the parent process.
 *
 * For example: the parent-process PPMM1 sends messages to the
 * child-process CPMM1.
 *
 * For example: CPMM1 sends messages directly to PPMM1. The global GPPMM
 * will also notify their message listeners when the message arrives.
 *
 * For example: messages sent through the global GPPMM will be
 * dispatched to the listeners of the same-process, CIPPMM, CPMM1,
 * CPMM2, etc.
 *
 *   ***** PERFORMANCE AND SECURITY WARNING *****
 * Messages broadcast through the GPPMM can result in messages
 * being dispatched across many OS processes, and to many processes
 * with different permissions.  Great care should be taken when
 * broadcasting.
 *
 * Requests sent to parent-process message listeners should usually
 * have replies scoped to the requesting CPMM.  The following pattern
 * is common
 *
 *  const ParentProcessListener = {
 *    receiveMessage: function(aMessage) {
 *      switch (aMessage.name) {
 *      case "Foo:Request":
 *        // service request
 *        aMessage.target.sendAsyncMessage("Foo:Response", { data });
 *      }
 *    }
 *  };
 */

dictionary ReceiveMessageArgument
{
  /**
   * The target of the message. Either an element owning the message manager, or message
   * manager itself if no element owns it.
   */
  required nsISupports target;

  /**
   * Message name.
   */
  required DOMString name;

  required boolean sync;

  /**
   * Structured clone of the sent message data
   */
  any data = null;

  /**
   * Same as .data, deprecated.
   */
  any json = null;

  /**
   * Named table of jsvals/objects, or null.
   */
  required object objects;

  sequence<MessagePort> ports;

  /**
   * Principal for the window app.
   */
  required Principal? principal;

  FrameLoader targetFrameLoader;
};

callback interface MessageListener
{
  /**
   * Each listener is invoked with its own copy of the message
   * parameter.
   *
   * When the listener is called, 'this' value is the target of the message.
   *
   * If the message is synchronous, the possible return value is
   * returned as JSON (will be changed to use structured clones).
   * When there are multiple listeners to sync messages, each
   * listener's return value is sent back as an array.  |undefined|
   * return values show up as undefined values in the array.
   */
  any receiveMessage(optional ReceiveMessageArgument argument);
};

[ChromeOnly]
interface MessageListenerManager
{
  /**
   * Register |listener| to receive |messageName|.  All listener
   * callbacks for a particular message are invoked when that message
   * is received.
   *
   * The message manager holds a strong ref to |listener|.
   *
   * If the same listener registers twice for the same message, the
   * second registration is ignored.
   *
   * Pass true for listenWhenClosed if you want to receive messages
   * during the short period after a frame has been removed from the
   * DOM and before its frame script has finished unloading. This
   * parameter only has an effect for frame message managers in
   * the main process. Default is false.
   */
  [Throws]
  void addMessageListener(DOMString messageName,
                          MessageListener listener,
                          optional boolean listenWhenClosed = false);

  /**
   * Undo an |addMessageListener| call -- that is, calling this causes us to no
   * longer invoke |listener| when |messageName| is received.
   *
   * removeMessageListener does not remove a message listener added via
   * addWeakMessageListener; use removeWeakMessageListener for that.
   */
  [Throws]
  void removeMessageListener(DOMString messageName,
                             MessageListener listener);

  /**
   * This is just like addMessageListener, except the message manager holds a
   * weak ref to |listener|.
   *
   * If you have two weak message listeners for the same message, they may be
   * called in any order.
   */
  [Throws]
  void addWeakMessageListener(DOMString messageName,
                              MessageListener listener);

  /**
   * This undoes an |addWeakMessageListener| call.
   */
  [Throws]
  void removeWeakMessageListener(DOMString messageName,
                                 MessageListener listener);
};

/**
 * Message "senders" have a single "other side" to which messages are
 * sent.  For example, a child-process message manager will send
 * messages that are only delivered to its one parent-process message
 * manager.
 */
[ChromeOnly]
interface MessageSender : MessageListenerManager
{
  /**
   * Send |messageName| and |obj| to the "other side" of this message
   * manager.  This invokes listeners who registered for
   * |messageName|.
   *
   * See ReceiveMessageArgument for the format of the data delivered to listeners.
   * @throws NS_ERROR_NOT_INITIALIZED if the sender is not initialized.  For
   *         example, we will throw NS_ERROR_NOT_INITIALIZED if we try to send
   *         a message to a cross-process frame but the other process has not
   *         yet been set up.
   * @throws NS_ERROR_FAILURE when the message receiver cannot be found.  For
   *         example, we will throw NS_ERROR_FAILURE if we try to send a message
   *         to a cross-process frame whose process has crashed.
   */
  [Throws]
  void sendAsyncMessage(optional DOMString? messageName = null,
                        optional any obj,
                        optional object? objects = null,
                        optional Principal? principal = null,
                        optional any transfers);

  /**
   * For remote browsers there is always a corresponding process message
   * manager. The intention of this attribute is to link leaf level frame
   * message managers on the parent side with the corresponding process
   * message managers (if there is one). For any other cases this property
   * is null.
   */
  [Throws]
  readonly attribute MessageSender? processMessageManager;

  /**
   * For remote browsers, this contains the remoteType of the content child.
   * Otherwise, it is empty.
   */
  [Throws]
  readonly attribute DOMString remoteType;
};

[ChromeOnly]
interface SyncMessageSender : MessageSender
{
  /**
   * Like |sendAsyncMessage()|, except blocks the sender until all
   * listeners of the message have been invoked.  Returns an array
   * containing return values from each listener invoked.
   */
  [Throws]
  sequence<any> sendSyncMessage(optional DOMString? messageName = null,
                                optional any obj,
                                optional object? objects = null,
                                optional Principal? principal = null);

  /**
   * Like |sendSyncMessage()|, except re-entrant. New RPC messages may be
   * issued even if, earlier on the call stack, we are waiting for a reply
   * to an earlier sendRpcMessage() call.
   *
   * Both sendSyncMessage and sendRpcMessage will block until a reply is
   * received, but they may be temporarily interrupted to process an urgent
   * incoming message (such as a CPOW request).
   */
  [Throws]
  sequence<any> sendRpcMessage(optional DOMString? messageName = null,
                               optional any obj,
                               optional object? objects = null,
                               optional Principal? principal = null);
};

/**
 * ChildProcessMessageManager is used in a child process to communicate with the parent
 * process.
 */
[ChromeOnly]
interface ChildProcessMessageManager : SyncMessageSender
{
};

[NoInterfaceObject]
interface MessageManagerGlobal : SyncMessageSender
{
  /**
   * Print a string to stdout.
   */
  void dump(DOMString str);

  /**
   * If leak detection is enabled, print a note to the leak log that this
   * process will intentionally crash.
   */
  [Throws]
  void privateNoteIntentionalCrash();

  /**
   * Ascii base64 data to binary data and vice versa
   */
  [Throws]
  DOMString atob(DOMString asciiString);
  [Throws]
  DOMString btoa(DOMString base64Data);
};

[NoInterfaceObject]
interface FrameScriptLoader
{
  /**
   * Load a script in the (remote) frame. |url| must be the absolute URL.
   * data: URLs are also supported. For example data:,dump("foo\n");
   * If allowDelayedLoad is true, script will be loaded when the
   * remote frame becomes available. Otherwise the script will be loaded
   * only if the frame is already available.
   */
  [Throws]
  void loadFrameScript(DOMString url, boolean allowDelayedLoad,
                       optional boolean runInGlobalScope = false);

  /**
   * Removes |url| from the list of scripts which support delayed load.
   */
  void removeDelayedFrameScript(DOMString url);

  /**
   * Returns all delayed scripts that will be loaded once a (remote)
   * frame becomes available. The return value is a list of pairs
   * [<URL>, <WasLoadedInGlobalScope>].
   */
  [Throws]
  sequence<sequence<any>> getDelayedFrameScripts();
};

[NoInterfaceObject]
interface ProcessScriptLoader
{
  /**
   * Load a script in the (possibly remote) process. |url| must be the absolute
   * URL. data: URLs are also supported. For example data:,dump("foo\n");
   * If allowDelayedLoad is true, script will be loaded when the
   * remote frame becomes available. Otherwise the script will be loaded
   * only if the frame is already available.
   */
  [Throws]
  void loadProcessScript(DOMString url, boolean allowDelayedLoad);

  /**
   * Removes |url| from the list of scripts which support delayed load.
   */
  void removeDelayedProcessScript(DOMString url);

  /**
   * Returns all delayed scripts that will be loaded once a (remote)
   * frame becomes available. The return value is a list of pairs
   * [<URL>, <WasLoadedInGlobalScope>].
   */
  [Throws]
  sequence<sequence<any>> getDelayedProcessScripts();
};

[NoInterfaceObject]
interface GlobalProcessScriptLoader : ProcessScriptLoader
{
  /**
   * Allows the parent process to set the initial process data for
   * new, not-yet-created child processes. This attribute should only
   * be used by the global parent process message manager. When a new
   * process is created, it gets a copy of this data (via structured
   * cloning). It can access the data via the initialProcessData
   * attribute of its childprocessmessagemanager.
   *
   * This value will always be a JS object if it's not null or undefined. Different
   * users are expected to set properties on this object. The property name should be
   * unique enough that other Gecko consumers won't accidentally choose it.
   */
  [Throws]
  readonly attribute any initialProcessData;

  readonly attribute MozWritableSharedMap sharedData;
};

[ChromeOnly, Global, NeedResolve]
interface ContentFrameMessageManager : EventTarget
{
  /**
   * The current top level window in the frame or null.
   */
  [Throws]
  readonly attribute WindowProxy? content;

  /**
   * The top level docshell or null.
   */
  [Throws]
  readonly attribute nsIDocShell? docShell;

  /**
   * Returns the SchedulerEventTarget corresponding to the TabGroup
   * for this frame.
   */
  readonly attribute nsIEventTarget? tabEventTarget;
};
// MessageManagerGlobal inherits from SyncMessageSender, which is a real interface, not a
// mixin. This will need to change when we implement mixins according to the current
// WebIDL spec.
ContentFrameMessageManager implements MessageManagerGlobal;

[ChromeOnly, Global, NeedResolve]
interface ContentProcessMessageManager
{
  /**
   * Read out a copy of the object that was initialized in the parent
   * process via ProcessScriptLoader.initialProcessData.
   */
  [Throws]
  readonly attribute any initialProcessData;

  readonly attribute MozSharedMap sharedData;
};
// MessageManagerGlobal inherits from SyncMessageSender, which is a real interface, not a
// mixin. This will need to change when we implement mixins according to the current
// WebIDL spec.
ContentProcessMessageManager implements MessageManagerGlobal;

/**
 * Message "broadcasters" don't have a single "other side" that they send messages to, but
 * rather a set of subordinate message managers. For example, broadcasting a message
 * through a window message manager will broadcast the message to all frame message
 * managers within its window.
 */
[ChromeOnly]
interface MessageBroadcaster : MessageListenerManager
{
  /**
   * Like |sendAsyncMessage()|, but also broadcasts this message to
   * all "child" message managers of this message manager.  See long
   * comment above for details.
   *
   * WARNING: broadcasting messages can be very expensive and leak
   * sensitive data.  Use with extreme caution.
   */
  [Throws]
  void broadcastAsyncMessage(optional DOMString? messageName = null,
                             optional any obj,
                             optional object? objects = null);

  /**
   * Number of subordinate message managers.
   */
  readonly attribute unsigned long childCount;

  /**
   * Return a single subordinate message manager.
   */
  MessageListenerManager getChildAt(unsigned long aIndex);

  /**
   * Some processes are kept alive after their last tab/window are closed for testing
   * (see dom.ipc.keepProcessesAlive). This function releases those.
   */
   void releaseCachedProcesses();
};

/**
 * ChromeMessageBroadcaster is used for window and group message managers.
 */
[ChromeOnly]
interface ChromeMessageBroadcaster : MessageBroadcaster
{
};
ChromeMessageBroadcaster implements FrameScriptLoader;

/**
 * ParentProcessMessageManager is used in a parent process to communicate with all the
 * child processes.
 */
[ChromeOnly]
interface ParentProcessMessageManager : MessageBroadcaster
{
};
ParentProcessMessageManager implements GlobalProcessScriptLoader;

[ChromeOnly]
interface ChromeMessageSender : MessageSender
{
};
ChromeMessageSender implements FrameScriptLoader;

/**
 * ProcessMessageManager is used in a parent process to communicate with a child process
 * (or with the process itself in a single-process scenario).
 */
[ChromeOnly]
interface ProcessMessageManager : MessageSender
{
};
ProcessMessageManager implements ProcessScriptLoader;
