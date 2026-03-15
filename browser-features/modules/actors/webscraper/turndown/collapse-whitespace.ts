/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Collapse whitespace functionality
 * Adapted from collapse-whitespace by Luc Thevenard
 *
 * The MIT License (MIT)
 * Copyright (c) 2014 Luc Thevenard <lucthevenard@gmail.com>
 */

interface CollapseWhitespaceOptions {
  element: Element;
  isBlock: (node: Node) => boolean;
  isVoid: (node: Node) => boolean;
  isPre: ((node: Node) => boolean) | null;
}

export default function collapseWhitespace(options: CollapseWhitespaceOptions): void {
  const element = options.element;
  const isBlock = options.isBlock;
  const isVoid = options.isVoid;
  const isPre =
    options.isPre ||
    function (node: Node) {
      return node.nodeName === "PRE";
    };

  if (!element.firstChild || isPre(element)) {
    return;
  }

  let prevText: Text | null = null;
  let keepLeadingWs = false;

  let prev: Node | null = null;
  let node = nextNode(prev, element, isPre);

  while (node !== element) {
    if (node.nodeType === Node.TEXT_NODE || node.nodeType === Node.CDATA_SECTION_NODE) {
      const textNode = node as Text;
      let text = textNode.data.replace(/[ \r\n\t]+/g, " ");

      if (
        (!prevText || / $/.test(prevText.data)) &&
        !keepLeadingWs &&
        text[0] === " "
      ) {
        text = text.substr(1);
      }

      // `text` might be empty at this point.
      if (!text) {
        node = removeNode(node);
        continue;
      }

      textNode.data = text;

      prevText = textNode;
    } else if (node.nodeType === Node.ELEMENT_NODE) {
      if (isBlock(node) || node.nodeName === "BR") {
        if (prevText) {
          prevText.data = prevText.data.replace(/ $/, "");
        }

        prevText = null;
        keepLeadingWs = false;
      } else if (isVoid(node) || isPre(node)) {
        // Avoid trimming space around non-block, non-BR void elements and inline PRE.
        prevText = null;
        keepLeadingWs = true;
      } else if (prevText) {
        // Drop protection if set previously.
        keepLeadingWs = false;
      }
    } else {
      node = removeNode(node);
      continue;
    }

    const nextNode_ = nextNode(prev, node, isPre);
    prev = node;
    node = nextNode_;
  }

  if (prevText) {
    prevText.data = prevText.data.replace(/ $/, "");
    if (!prevText.data) {
      removeNode(prevText);
    }
  }
}

/**
 * remove(node) removes the given node from the DOM and returns the
 * next node in the sequence.
 */
function removeNode(node: Node): Node {
  const next = node.nextSibling || node.parentNode;

  if (node.parentNode) {
    node.parentNode.removeChild(node);
  }

  return next!;
}

/**
 * next(prev, current, isPre) returns the next node in the sequence, given the
 * current and previous nodes.
 */
function nextNode(
  prev: Node | null,
  current: Node,
  isPre: (node: Node) => boolean,
): Node {
  if ((prev && prev.parentNode === current) || isPre(current)) {
    return current.nextSibling || current.parentNode!;
  }

  return current.firstChild || current.nextSibling || current.parentNode!;
}
