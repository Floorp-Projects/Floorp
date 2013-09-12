/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org
 */

interface MutationRecord {
  readonly attribute DOMString type;
  // .target is not nullable per the spec, but in order to prevent crashes,
  // if there are GC/CC bugs in Gecko, we let the property to be null.
  readonly attribute Node? target;
  readonly attribute NodeList addedNodes;
  readonly attribute NodeList removedNodes;
  readonly attribute Node? previousSibling;
  readonly attribute Node? nextSibling;
  readonly attribute DOMString? attributeName;
  readonly attribute DOMString? attributeNamespace;
  readonly attribute DOMString? oldValue;
};

[Constructor(MutationCallback mutationCallback)]
interface MutationObserver {
  [Throws]
  void observe(Node target, optional MutationObserverInit options);
  void disconnect();
  sequence<MutationRecord> takeRecords();

  [ChromeOnly]
  sequence<MutationObservingInfo?> getObservingInfo();
  [ChromeOnly]
  readonly attribute MutationCallback mutationCallback;
};

callback MutationCallback = void (sequence<MutationRecord> mutations, MutationObserver observer);

dictionary MutationObserverInit {
  boolean childList = false;
  boolean attributes = false;
  boolean characterData = false;
  boolean subtree = false;
  boolean attributeOldValue = false;
  boolean characterDataOldValue = false;
  sequence<DOMString> attributeFilter;
};

dictionary MutationObservingInfo : MutationObserverInit
{
  Node? observedNode = null;
};
