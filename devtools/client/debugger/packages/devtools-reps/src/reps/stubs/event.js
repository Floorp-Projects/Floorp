/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();

stubs.set("testEvent", {
  type: "object",
  class: "Event",
  actor: "server1.conn23.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMEvent",
    type: "beforeprint",
    properties: {
      isTrusted: true,
      currentTarget: {
        type: "object",
        class: "Window",
        actor: "server1.conn23.obj37",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 760,
        preview: {
          kind: "ObjectWithURL",
          url: "http://example.com",
        },
      },
      eventPhase: 2,
      bubbles: false,
      cancelable: false,
      defaultPrevented: false,
      timeStamp: 1466780008434005,
      originalTarget: {
        type: "object",
        class: "Window",
        actor: "server1.conn23.obj38",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 760,
        preview: {
          kind: "ObjectWithURL",
          url: "http://example.com",
        },
      },
      explicitOriginalTarget: {
        type: "object",
        class: "Window",
        actor: "server1.conn23.obj39",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 760,
        preview: {
          kind: "ObjectWithURL",
          url: "http://example.com",
        },
      },
      NONE: 0,
    },
    target: {
      type: "object",
      class: "Window",
      actor: "server1.conn23.obj36",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 760,
      preview: {
        kind: "ObjectWithURL",
        url: "http://example.com",
      },
    },
  },
});

stubs.set("testMouseEvent", {
  type: "object",
  class: "MouseEvent",
  actor: "server1.conn20.obj39",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMEvent",
    type: "click",
    properties: {
      buttons: 0,
      clientX: 62,
      clientY: 18,
      layerX: 0,
      layerY: 0,
    },
    target: {
      type: "object",
      class: "HTMLDivElement",
      actor: "server1.conn20.obj40",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 0,
      preview: {
        kind: "DOMNode",
        nodeType: 1,
        nodeName: "div",
        isConnected: true,
        attributes: {
          id: "test",
        },
        attributesLength: 1,
      },
    },
  },
});

stubs.set("testKeyboardEvent", {
  type: "object",
  class: "KeyboardEvent",
  actor: "server1.conn21.obj49",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMEvent",
    type: "keyup",
    properties: {
      key: "Control",
      charCode: 0,
      keyCode: 17,
    },
    target: {
      type: "object",
      class: "HTMLBodyElement",
      actor: "server1.conn21.obj50",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 0,
      preview: {
        kind: "DOMNode",
        nodeType: 1,
        nodeName: "body",
        attributes: {},
        attributesLength: 0,
      },
    },
    eventKind: "key",
    modifiers: [],
  },
});

stubs.set("testKeyboardEventWithModifiers", {
  type: "object",
  class: "KeyboardEvent",
  actor: "server1.conn21.obj49",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMEvent",
    type: "keyup",
    properties: {
      key: "M",
      charCode: 0,
      keyCode: 77,
    },
    target: {
      type: "object",
      class: "HTMLBodyElement",
      actor: "server1.conn21.obj50",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 0,
      preview: {
        kind: "DOMNode",
        nodeType: 1,
        nodeName: "body",
        attributes: {},
        attributesLength: 0,
      },
    },
    eventKind: "key",
    modifiers: ["Meta", "Shift"],
  },
});

stubs.set("testMessageEvent", {
  type: "object",
  class: "MessageEvent",
  actor: "server1.conn3.obj34",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMEvent",
    type: "message",
    properties: {
      isTrusted: false,
      data: "test data",
      origin: "null",
      lastEventId: "",
      source: {
        type: "object",
        class: "Window",
        actor: "server1.conn3.obj36",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 760,
        preview: {
          kind: "ObjectWithURL",
          url: "",
        },
      },
      ports: {
        type: "object",
        class: "Array",
        actor: "server1.conn3.obj37",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 0,
      },
      currentTarget: {
        type: "object",
        class: "Window",
        actor: "server1.conn3.obj38",
        extensible: true,
        frozen: false,
        sealed: false,
        ownPropertyLength: 760,
        preview: {
          kind: "ObjectWithURL",
          url: "",
        },
      },
      eventPhase: 2,
      bubbles: false,
      cancelable: false,
    },
    target: {
      type: "object",
      class: "Window",
      actor: "server1.conn3.obj35",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 760,
      preview: {
        kind: "ObjectWithURL",
        url: "http://example.com",
      },
    },
  },
});

module.exports = stubs;
