/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const stubs = new Map();
stubs.set("1n", {
  type: "BigInt",
  text: "1",
});

stubs.set("-2n", {
  type: "BigInt",
  text: "-2",
});

stubs.set("0n", {
  type: "BigInt",
  text: "0",
});

stubs.set("[1n,-2n,0n]", {
  type: "object",
  actor: "server1.conn15.child1/obj27",
  class: "Array",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 4,
  preview: {
    kind: "ArrayLike",
    length: 3,
    items: [
      {
        type: "BigInt",
        text: "1",
      },
      {
        type: "BigInt",
        text: "-2",
      },
      {
        type: "BigInt",
        text: "0",
      },
    ],
  },
});

stubs.set("new Set([1n,-2n,0n])", {
  type: "object",
  actor: "server1.conn15.child1/obj29",
  class: "Set",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "ArrayLike",
    length: 3,
    items: [
      {
        type: "BigInt",
        text: "1",
      },
      {
        type: "BigInt",
        text: "-2",
      },
      {
        type: "BigInt",
        text: "0",
      },
    ],
  },
});

stubs.set("new Map([ [1n, -1n], [-2n, 0n], [0n, -2n]])", {
  type: "object",
  actor: "server1.conn15.child1/obj32",
  class: "Map",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "MapLike",
    size: 3,
    entries: [
      [
        {
          type: "BigInt",
          text: "1",
        },
        {
          type: "BigInt",
          text: "-1",
        },
      ],
      [
        {
          type: "BigInt",
          text: "-2",
        },
        {
          type: "BigInt",
          text: "0",
        },
      ],
      [
        {
          type: "BigInt",
          text: "0",
        },
        {
          type: "BigInt",
          text: "-2",
        },
      ],
    ],
  },
});

stubs.set("({simple: 1n, negative: -2n, zero: 0n})", {
  type: "object",
  actor: "server1.conn15.child1/obj34",
  class: "Object",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 3,
  preview: {
    kind: "Object",
    ownProperties: {
      simple: {
        configurable: true,
        enumerable: true,
        writable: true,
        value: {
          type: "BigInt",
          text: "1",
        },
      },
      negative: {
        configurable: true,
        enumerable: true,
        writable: true,
        value: {
          type: "BigInt",
          text: "-2",
        },
      },
      zero: {
        configurable: true,
        enumerable: true,
        writable: true,
        value: {
          type: "BigInt",
          text: "0",
        },
      },
    },
    ownSymbols: [],
    ownPropertiesLength: 3,
    ownSymbolsLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("Promise.resolve(1n)", {
  type: "object",
  actor: "server1.conn15.child1/obj36",
  class: "Promise",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
        value: {
          type: "BigInt",
          text: "1",
        },
      },
    },
    ownPropertiesLength: 2,
  },
});

module.exports = stubs;
