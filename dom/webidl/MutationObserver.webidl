/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org
 */

interface MutationRecord {
  [Constant]
  readonly attribute DOMString type;
  // .target is not nullable per the spec, but in order to prevent crashes,
  // if there are GC/CC bugs in Gecko, we let the property to be null.
  [Constant]
  readonly attribute Node? target;
  [Constant]
  readonly attribute NodeList addedNodes;
  [Constant]
  readonly attribute NodeList removedNodes;
  [Constant]
  readonly attribute Node? previousSibling;
  [Constant]
  readonly attribute Node? nextSibling;
  [Constant]
  readonly attribute DOMString? attributeName;
  [Constant]
  readonly attribute DOMString? attributeNamespace;
  [Constant]
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
  boolean attributes;
  boolean characterData;
  boolean subtree = false;
  boolean attributeOldValue;
  boolean characterDataOldValue;
  sequence<DOMString> attributeFilter;
};

dictionary MutationObservingInfo : MutationObserverInit
{
  Node? observedNode = null;
};
