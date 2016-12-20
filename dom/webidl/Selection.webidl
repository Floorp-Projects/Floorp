/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/editing/raw-file/tip/editing.html#concept-selection
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Selection {
  readonly attribute Node? anchorNode;
  readonly attribute unsigned long anchorOffset;
  readonly attribute Node? focusNode;
  readonly attribute unsigned long focusOffset;

  readonly attribute boolean isCollapsed;
  [Throws]
  void               collapse(Node node, unsigned long offset);
  [Throws]
  void               collapseToStart();
  [Throws]
  void               collapseToEnd();

  [Throws]
  void               extend(Node node, unsigned long offset);

  [Throws]
  void               selectAllChildren(Node node);
  [Throws]
  void               deleteFromDocument();

  readonly attribute unsigned long rangeCount;
  [Throws]
  Range              getRangeAt(unsigned long index);
  [Throws]
  void               addRange(Range range);
  [Throws]
  void               removeRange(Range range);
  [Throws]
  void               removeAllRanges();

  [Throws]
  boolean            containsNode(Node node, boolean allowPartialContainment);

  [Throws]
  void               setBaseAndExtent(Node anchorNode,
                                      unsigned long anchorOffset,
                                      Node focusNode,
                                      unsigned long focusOffset);

  stringifier;
};

// Additional methods not currently in the spec
partial interface Selection {
  [Throws]
  void modify(DOMString alter, DOMString direction,
              DOMString granularity);
};

// Additional chrome-only methods from nsISelectionPrivate
interface nsISelectionListener;
partial interface Selection {
  [ChromeOnly]
  const short ENDOFPRECEDINGLINE = 0;
  [ChromeOnly]
  const short STARTOFNEXTLINE = 1;

  [ChromeOnly,Throws]
  attribute boolean interlinePosition;

  [Throws]
  attribute short? caretBidiLevel;

  [ChromeOnly,Throws]
  DOMString  toStringWithFormat(DOMString formatType, unsigned long flags, long wrapColumn);
  [ChromeOnly,Throws]
  void  addSelectionListener(nsISelectionListener newListener);
  [ChromeOnly,Throws]
  void  removeSelectionListener(nsISelectionListener listenerToRemove);

  [ChromeOnly,BinaryName="rawType"]
  readonly attribute short type;

  [ChromeOnly,Throws,Pref="dom.testing.selection.GetRangesForInterval"]
  sequence<Range> GetRangesForInterval(Node beginNode, long beginOffset, Node endNode, long endOffset,
                                       boolean allowAdjacent);

  [ChromeOnly,Throws]
  void scrollIntoView(short aRegion, boolean aIsSynchronous, short aVPercent, short aHPercent);
};
