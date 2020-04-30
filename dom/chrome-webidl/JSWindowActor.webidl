/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An actor architecture designed to allow compositional parent/content
 * communications. The lifetime of a JSWindowActor{Child, Parent} is the `WindowGlobalParent`
 * (for the parent-side) / `WindowGlobalChild` (for the child-side).
 *
 * See https://firefox-source-docs.mozilla.org/dom/Fission.html#jswindowactor for
 * more details on how to use this architecture.
 */

interface nsISupports;

[ChromeOnly, Exposed=Window]
interface JSWindowActorParent {
  [ChromeOnly]
  constructor();

  /**
   * Actor initialization occurs after the constructor is called but before the
   * first message is delivered. Until the actor is initialized, accesses to
   * manager will fail.
   */
  readonly attribute WindowGlobalParent? manager;

  [Throws]
  readonly attribute CanonicalBrowsingContext? browsingContext;
};
JSWindowActorParent includes JSActor;

[ChromeOnly, Exposed=Window]
interface JSWindowActorChild {
  [ChromeOnly]
  constructor();

  /**
   * Actor initialization occurs after the constructor is called but before the
   * first message is delivered. Until the actor is initialized, accesses to
   * manager will fail.
   */
  readonly attribute WindowGlobalChild? manager;

  [Throws]
  readonly attribute Document? document;

  [Throws]
  readonly attribute BrowsingContext? browsingContext;

  [Throws]
  readonly attribute nsIDocShell? docShell;

  /**
   * NOTE: As this returns a window proxy, it may not be currently referencing
   * the document associated with this JSWindowActor. Generally prefer using
   * `document`.
   */
  [Throws]
  readonly attribute WindowProxy? contentWindow;
};
JSWindowActorChild includes JSActor;

/**
 * Used by ChromeUtils.registerWindowActor() to register JS window actor.
 */
dictionary WindowActorOptions {
  /**
   * If this is set to `true`, allow this actor to be created for subframes,
   * and not just toplevel window globals.
   */
  boolean allFrames = false;

  /**
   * If this is set to `true`, allow this actor to be created for window
   * globals loaded in chrome browsing contexts, such as those used to load the
   * tabbrowser.
   */
  boolean includeChrome = false;

  /**
   * An array of URL match patterns (as accepted by the MatchPattern
   * class in MatchPattern.webidl) which restrict which pages the actor
   * may be instantiated for. If this is defined, only documents URL which match
   * are allowed to have the given actor created for them. Other
   * documents will fail to have their actor constructed, returning nullptr.
   */
  sequence<DOMString> matches;

  /**
   * An array of remote type which restricts the actor is allowed to instantiate
   * in specific process type. If this is defined, the prefix of process type
   * matches the remote type by prefix match is allowed to instantiate, ex: if
   * Fission is enabled, the prefix of process type will be `webIsolated`, it
   * can prefix match remote type either `web` or `webIsolated`. If not passed,
   * all content processes are allowed to instantiate the actor.
   */
  sequence<DOMString> remoteTypes;

  /**
   * An array of MessageManagerGroup values which restrict which type
   * of browser elements the actor is allowed to be loaded within.
   */
  sequence<DOMString> messageManagerGroups;

  /** This fields are used for configuring individual sides of the actor. */
  WindowActorSidedOptions parent;
  WindowActorChildOptions child;
};

dictionary WindowActorSidedOptions {
  /**
   * The JSM path which should be loaded for the actor on this side.
   * If not passed, the specified side cannot receive messages, but may send
   * them using `sendAsyncMessage` or `sendQuery`.
   */
  required ByteString moduleURI;
};

dictionary WindowActorChildOptions : WindowActorSidedOptions {
  /**
   * Events which this actor wants to be listening to. When these events fire,
   * it will trigger actor creation, and then forward the event to the actor.
   *
   * NOTE: Listeners are not attached for windows loaded in chrome docshells.
   *
   * NOTE: `once` option is not support due to we register listeners in a shared
   * location.
   */
  record<DOMString, AddEventListenerOptions> events;

 /**
  * An array of observer topics to listen to. An observer will be added for each
  * topic in the list.
  *
  * Observer notifications in the list use nsGlobalWindowInner or
  * nsGlobalWindowOuter object as their subject, and the events will only be
  * dispatched to the corresponding window actor. If additional observer
  * notification's subjects are needed, please file a bug for that.
  **/
  sequence<ByteString> observers;
};
