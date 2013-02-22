/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-dom-20120105/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Range {
  readonly attribute Node startContainer;
  readonly attribute unsigned long startOffset;
  readonly attribute Node endContainer;
  readonly attribute unsigned long endOffset;
  readonly attribute boolean collapsed;
  readonly attribute Node commonAncestorContainer;

  void setStart(Node refNode, unsigned long offset);
  void setEnd(Node refNode, unsigned long offset);
  void setStartBefore(Node refNode);
  void setStartAfter(Node refNode);
  void setEndBefore(Node refNode);
  void setEndAfter(Node refNode);
  void collapse(boolean toStart);
  void selectNode(Node refNode);
  void selectNodeContents(Node refNode);

  const unsigned short START_TO_START = 0;
  const unsigned short START_TO_END = 1;
  const unsigned short END_TO_END = 2;
  const unsigned short END_TO_START = 3;
  short compareBoundaryPoints(unsigned short how, Range sourceRange);

  void deleteContents();
  DocumentFragment extractContents();
  DocumentFragment cloneContents();
  void insertNode(Node node);
  void surroundContents(Node newParent);

  Range cloneRange();
  void detach();

  boolean isPointInRange(Node node, unsigned long offset);
  short comparePoint(Node node, unsigned long offset);

  boolean intersectsNode(Node node);

  stringifier;
};
