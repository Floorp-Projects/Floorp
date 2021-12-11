"use strict";

let global = this;
let p = {};
let q = {};

let g1 = function() {
  assertEq(this, undefined);
};
g1(...[]);

let g2 = x => {
  assertEq(this, global);
};
g2(...[]);

let g3 = function() {
  assertEq(this, p);
};
g3.apply(p, ...[]);
g3.call(p, ...[]);

g2.apply(p, ...[]);
g2.call(p, ...[]);

let o = {
  f1: function() {
    assertEq(this, o);

    let g1 = function() {
      assertEq(this, undefined);
    };
    g1(...[]);

    let g2 = x => {
      assertEq(this, o);
    };
    g2(...[]);

    let g3 = function() {
      assertEq(this, q);
    };
    g3.apply(q, ...[]);
    g3.call(q, ...[]);

    let g4 = x => {
      assertEq(this, o);
    };
    g4.apply(q, ...[]);
    g4.call(q, ...[]);
  },
  f2: x => {
    assertEq(this, global);
    let g1 = function() {
      assertEq(this, undefined);
    };
    g1(...[]);

    let g2 = x => {
      assertEq(this, global);
    };
    g2(...[]);

    let g3 = function() {
      assertEq(this, q);
    };
    g3.apply(q, ...[]);
    g3.call(q, ...[]);

    let g4 = x => {
      assertEq(this, global);
    };
    g4.apply(q, ...[]);
    g4.call(q, ...[]);
  },
  f3: function() {
    assertEq(this, p);

    let g1 = function() {
      assertEq(this, undefined);
    };
    g1(...[]);

    let g2 = x => {
      assertEq(this, p);
    };
    g2(...[]);

    let g3 = function() {
      assertEq(this, q);
    };
    g3.apply(q, ...[]);
    g3.call(q, ...[]);

    let g4 = x => {
      assertEq(this, p);
    };
    g4.apply(q, ...[]);
    g4.call(q, ...[]);
  }
};
o.f1(...[]);
o.f2(...[]);
o.f3.apply(p, ...[]);
o.f2.apply(p, ...[]);
