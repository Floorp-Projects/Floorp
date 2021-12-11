/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An actor architecture designed to allow compositional parent/content
 * communications. The lifetime of a JSProcessActor{Child, Parent} is the `ContentParent`
 * (for the parent-side) / `ContentChild` (for the child-side).
 */

interface nsISupports;

/**
 * Base class for parent-side actor.
 */
[ChromeOnly, Exposed=Window]
interface JSProcessActorParent {
  [ChromeOnly]
  constructor();

  readonly attribute nsIDOMProcessParent manager;
};
JSProcessActorParent includes JSActor;

[ChromeOnly, Exposed=Window]
interface JSProcessActorChild {
  [ChromeOnly]
  constructor();

  readonly attribute nsIDOMProcessChild manager;
};
JSProcessActorChild includes JSActor;


/**
 * Used by `ChromeUtils.registerProcessActor()` to register actors.
 */
dictionary ProcessActorOptions {
  /**
   * An array of remote type which restricts the actor is allowed to instantiate
   * in specific process type. If this is defined, the prefix of process type
   * matches the remote type by prefix match is allowed to instantiate, ex: if
   * Fission is enabled, the prefix of process type will be `webIsolated`, it
   * can prefix match remote type either `web` or `webIsolated`. If not passed,
   * all content processes are allowed to instantiate the actor.
   */
  sequence<UTF8String> remoteTypes;

  /**
   * If this is set to `true`, allow this actor to be created for the parent
   * process.
   */
  boolean includeParent = false;

  /** This fields are used for configuring individual sides of the actor. */
  ProcessActorSidedOptions parent;
  ProcessActorChildOptions child;
};

dictionary ProcessActorSidedOptions {
  /**
   * The JSM path which should be loaded for the actor on this side.
   * If not passed, the specified side cannot receive messages, but may send
   * them using `sendAsyncMessage` or `sendQuery`.
   */
  required ByteString moduleURI;
};

dictionary ProcessActorChildOptions : ProcessActorSidedOptions {
  /**
   * An array of observer topics to listen to. An observer will be added for each
   * topic in the list.
   *
   * Unlike for JSWindowActor, observers are always invoked, and do not need to
   * pass an inner or outer window as subject.
   */
  sequence<ByteString> observers;
};
