/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("BodyNode", {
  type: "object",
  actor: "server1.conn1.child1/obj30",
  class: "HTMLBodyElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "body",
    attributes: {
      class: "body-class",
      id: "body-id",
    },
    attributesLength: 2,
  },
});

stubs.set("DocumentElement", {
  type: "object",
  actor: "server1.conn1.child1/obj40",
  class: "HTMLHtmlElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "html",
    attributes: {
      dir: "ltr",
      lang: "en-US",
    },
    attributesLength: 2,
  },
});

stubs.set("Node", {
  type: "object",
  actor: "server1.conn2.child1/obj116",
  class: "HTMLInputElement",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "input",
    isConnected: true,
    attributes: {
      id: "newtab-customize-button",
      dir: "ltr",
      title: "Customize your New Tab page",
      class: "bar baz",
      value: "foo",
      type: "button",
    },
    attributesLength: 6,
  },
});

stubs.set("DisconnectedNode", {
  type: "object",
  actor: "server1.conn2.child1/obj116",
  class: "HTMLInputElement",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "input",
    isConnected: false,
    attributes: {
      id: "newtab-customize-button",
      dir: "ltr",
      title: "Customize your New Tab page",
      class: "bar baz",
      value: "foo",
      type: "button",
    },
    attributesLength: 6,
  },
});

stubs.set("NodeWithLeadingAndTrailingSpacesClassName", {
  type: "object",
  actor: "server1.conn3.child1/obj59",
  class: "HTMLBodyElement",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "body",
    attributes: {
      id: "nightly-whatsnew",
      class: "  html-ltr    ",
    },
    attributesLength: 2,
  },
});

stubs.set("NodeWithSpacesInClassName", {
  type: "object",
  actor: "server1.conn3.child1/obj59",
  class: "HTMLBodyElement",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "body",
    attributes: {
      class: "a  b   c",
    },
    attributesLength: 1,
  },
});

stubs.set("NodeWithoutAttributes", {
  type: "object",
  actor: "server1.conn1.child1/obj32",
  class: "HTMLParagraphElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "p",
    attributes: {},
    attributesLength: 1,
  },
});

stubs.set("LotsOfAttributes", {
  type: "object",
  actor: "server1.conn2.child1/obj30",
  class: "HTMLParagraphElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "p",
    attributes: {
      id: "lots-of-attributes",
      a: "",
      b: "",
      c: "",
      d: "",
      e: "",
      f: "",
      g: "",
      h: "",
      i: "",
      j: "",
      k: "",
      l: "",
      m: "",
      n: "",
    },
    attributesLength: 15,
  },
});

stubs.set("SvgNode", {
  type: "object",
  actor: "server1.conn1.child1/obj42",
  class: "SVGClipPathElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "clipPath",
    attributes: {
      id: "clip",
      class: "svg-element",
    },
    attributesLength: 0,
  },
});

stubs.set("SvgNodeInXHTML", {
  type: "object",
  actor: "server1.conn3.child1/obj34",
  class: "SVGCircleElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "svg:circle",
    attributes: {
      class: "svg-element",
      cx: "0",
      cy: "0",
      r: "5",
    },
    attributesLength: 3,
  },
});

stubs.set("NodeWithLongAttribute", {
  type: "object",
  actor: "server1.conn1.child1/obj32",
  class: "HTMLParagraphElement",
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "p",
    attributes: {
      "data-test": "a".repeat(100),
    },
    attributesLength: 1,
  },
});

const initialText = "a".repeat(1000);
stubs.set("NodeWithLongStringAttribute", {
  type: "object",
  actor: "server1.conn1.child1/obj28",
  class: "HTMLDivElement",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "div",
    isConnected: false,
    attributes: {
      "data-test": {
        type: "longString",
        initial: initialText,
        length: 50000,
        actor: "server1.conn1.child1/longString29",
      },
    },
    attributesLength: 1,
  },
});

stubs.set("MarkerPseudoElement", {
  type: "object",
  actor: "server1.conn1.child1/obj26",
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "_moz_generated_content_marker",
    attributes: {},
    attributesLength: 0,
    isMarkerPseudoElement: true,
  },
});

stubs.set("BeforePseudoElement", {
  type: "object",
  actor: "server1.conn1.child1/obj27",
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "_moz_generated_content_before",
    attributes: {},
    attributesLength: 0,
    isBeforePseudoElement: true,
  },
});

stubs.set("AfterPseudoElement", {
  type: "object",
  actor: "server1.conn1.child1/obj28",
  preview: {
    kind: "DOMNode",
    nodeType: 1,
    nodeName: "_moz_generated_content_after",
    attributes: {},
    attributesLength: 0,
    isAfterPseudoElement: true,
  },
});

module.exports = stubs;
