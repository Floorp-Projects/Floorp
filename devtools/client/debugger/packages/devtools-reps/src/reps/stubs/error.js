/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("SimpleError", {
  type: "object",
  actor: "server1.conn1.child1/obj1020",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "Error message",
    stack: "@debugger eval code:1:13\n",
    fileName: "debugger eval code",
    lineNumber: 1,
    columnNumber: 13,
  },
});

stubs.set("MultilineStackError", {
  type: "object",
  actor: "server1.conn1.child1/obj1021",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "bar",
    stack:
      "errorBar@debugger eval code:6:15\n" +
      "errorFoo@debugger eval code:3:3\n" +
      "@debugger eval code:8:1\n",
    fileName: "debugger eval code",
    lineNumber: 6,
    columnNumber: 15,
  },
});

stubs.set("ErrorWithoutStacktrace", {
  type: "object",
  actor: "server1.conn1.child1/obj1020",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "Error message",
  },
});

stubs.set("EvalError", {
  type: "object",
  actor: "server1.conn1.child1/obj1022",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "EvalError",
    message: "EvalError message",
    stack: "@debugger eval code:10:13\n",
    fileName: "debugger eval code",
    lineNumber: 10,
    columnNumber: 13,
  },
});

stubs.set("InternalError", {
  type: "object",
  actor: "server1.conn1.child1/obj1023",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "InternalError",
    message: "InternalError message",
    stack: "@debugger eval code:11:13\n",
    fileName: "debugger eval code",
    lineNumber: 11,
    columnNumber: 13,
  },
});

stubs.set("RangeError", {
  type: "object",
  actor: "server1.conn1.child1/obj1024",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "RangeError",
    message: "RangeError message",
    stack: "@debugger eval code:12:13\n",
    fileName: "debugger eval code",
    lineNumber: 12,
    columnNumber: 13,
  },
});

stubs.set("ReferenceError", {
  type: "object",
  actor: "server1.conn1.child1/obj1025",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "ReferenceError",
    message: "ReferenceError message",
    stack: "@debugger eval code:13:13\n",
    fileName: "debugger eval code",
    lineNumber: 13,
    columnNumber: 13,
  },
});

stubs.set("SyntaxError", {
  type: "object",
  actor: "server1.conn1.child1/obj1026",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "SyntaxError",
    message: "SyntaxError message",
    stack: "@debugger eval code:14:13\n",
    fileName: "debugger eval code",
    lineNumber: 14,
    columnNumber: 13,
  },
});

stubs.set("TypeError", {
  type: "object",
  actor: "server1.conn1.child1/obj1027",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "TypeError",
    message: "TypeError message",
    stack: "@debugger eval code:15:13\n",
    fileName: "debugger eval code",
    lineNumber: 15,
    columnNumber: 13,
  },
});

stubs.set("URIError", {
  type: "object",
  actor: "server1.conn1.child1/obj1028",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "URIError",
    message: "URIError message",
    stack: "@debugger eval code:16:13\n",
    fileName: "debugger eval code",
    lineNumber: 16,
    columnNumber: 13,
  },
});

/**
 * Example code:
 *  try {
 *    var foo = document.querySelector("foo;()bar!");
 *  } catch (ex) {
 *    ex;
 *  }
 */
stubs.set("DOMException", {
  type: "object",
  actor: "server2.conn2.child3/obj32",
  class: "DOMException",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMException",
    name: "SyntaxError",
    message: "'foo;()bar!' is not a valid selector",
    code: 12,
    result: 2152923148,
    filename: "debugger eval code",
    lineNumber: 1,
    columnNumber: 0,
  },
});

stubs.set("base-loader Error", {
  type: "object",
  actor: "server1.conn1.child1/obj1020",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "Error message",
    stack:
      "onPacket@resource://devtools/shared/base-loader.js -> resource://devtools/client/debugger-client.js:856:9\n" +
      "send/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/transport/transport.js:569:13\n" +
      "exports.makeInfallible/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/ThreadSafeDevToolsUtils.js:109:14\n" +
      "exports.makeInfallible/<@resource://devtools/shared/base-loader.js -> resource://devtools/shared/ThreadSafeDevToolsUtils.js:109:14\n",
    fileName: "debugger-client.js",
    lineNumber: 859,
    columnNumber: 9,
  },
});

stubs.set("longString stack Error", {
  type: "object",
  actor: "server1.conn2.child1/obj33",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "",
    stack: {
      type: "longString",
      initial:
        "NgForOf.prototype.ngOnChanges@webpack-internal:///./node_modules/@angular/common/esm5/common.js:2656:27\n checkAndUpdateDirectiveInline@webpack-internal:///./node_modules/@angular/core/esm5/core.js:12581:9\n checkAndUpdateNodeInline@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14109:20\n checkAndUpdateNode@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14052:16\n debugCheckAndUpdateNode@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14945:55\n debugCheckDirectivesFn@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14886:13\n View_MetaTableComponent_6/<@ng:///AppModule/MetaTableComponent.ngfactory.js:98:5\n debugUpdateDirectives@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14871:12\n checkAndUpdateView@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14018:5\n callViewAction@webpack-internal:///./node_modules/@angular/core/esm5/core.js:14369:21\n execEmbeddedViewsAction@webpack-internal:///./node_modules/@an",
      length: 11907,
      actor: "server1.conn2.child1/longString31",
    },
    fileName: "debugger eval code",
    lineNumber: 1,
    columnNumber: 5,
  },
});

stubs.set("longString stack Error - cut-off location", {
  type: "object",
  actor: "server1.conn1.child1/obj33",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 6,
  preview: {
    kind: "Error",
    name: "InternalError",
    message: "too much recursion",
    stack: {
      type: "longString",
      initial:
        "execute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:32:1\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://angular-3eqab4.stackblitz.io/tmp/appfiles/src/app/app.component.ts:33:21\nexecute/AppComponent</AppComponent.prototype.doStuff@https://an",
      length: 17151,
      actor: "server1.conn1.child1/longString27",
    },
    fileName:
      "https://c.staticblitz.com/assets/engineblock-bc7b07e99ec5c6739c766b4898e4cff5acfddc137ccb7218377069c32731f1d0.js line 1 > eval",
    lineNumber: 32,
    columnNumber: 1,
  },
});

stubs.set("Error with V8-like stack", {
  type: "object",
  actor: "server1.conn1.child1/obj1020",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "BOOM",
    stack: "Error: BOOM\ngetAccount@http://moz.com/script.js:1:2",
    fileName: "http://moz.com/script.js:1:2",
    lineNumber: 1,
    columnNumber: 2,
  },
});

stubs.set("Error with invalid stack", {
  type: "object",
  actor: "server1.conn1.child1/obj1020",
  class: "Error",
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "Error",
    message: "bad stack",
    stack: "bar\nbaz\nfoo\n\n\n\n\n\n\n",
    fileName: "http://moz.com/script.js:1:2",
    lineNumber: 1,
    columnNumber: 2,
  },
});

stubs.set("Error with undefined-grip stack", {
  type: "object",
  actor: "server0.conn0.child1/obj88",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: "InternalError",
    message: "too much recursion",
    stack: {
      type: "undefined",
    },
    fileName: "debugger eval code",
    lineNumber: 13,
    columnNumber: 13,
  },
});

stubs.set("Error with undefined-grip name", {
  type: "object",
  actor: "server0.conn0.child1/obj88",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    name: {
      type: "undefined",
    },
    message: "too much recursion",
    stack: "@debugger eval code:16:13\n",
    fileName: "debugger eval code",
    lineNumber: 13,
    columnNumber: 13,
  },
});

stubs.set("Error with undefined-grip message", {
  type: "object",
  actor: "server0.conn0.child1/obj88",
  class: "Error",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "Error",
    message: { type: "undefined" },
    stack: "@debugger eval code:16:13\n",
    fileName: "debugger eval code",
    lineNumber: 13,
    columnNumber: 13,
  },
});

module.exports = stubs;
