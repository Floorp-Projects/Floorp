function test_syntax(postfixes, check_error, ignore_opts) {
  function test_reflect(code, module) {
    var options = undefined;
    if (module) {
      options = {target: "module"};
    }
    for (var postfix of postfixes) {
      var cur_code = code + postfix;

      var caught = false;
      try {
        Reflect.parse(cur_code, options);
      } catch (e) {
        caught = true;
        check_error(e, cur_code, "reflect");
      }
      assertEq(caught, true);
    }
  }

  function test_eval(code) {
    for (var postfix of postfixes) {
      var cur_code = code + postfix;

      var caught = false;
      try {
        eval(cur_code);
      } catch (e) {
        caught = true;
        check_error(e, cur_code, "eval");
      }
      assertEq(caught, true);
    }
  }

  function test(code, opts={}) {
    if (ignore_opts) {
      opts = {};
    }

    let no_strict = "no_strict" in opts && opts.no_strict;
    let no_fun = "no_fun" in opts && opts.no_fun;
    let no_eval = "no_eval" in opts && opts.no_eval;
    let module = "module" in opts && opts.module;

    test_reflect(code, module);
    if (!no_strict) {
      test_reflect("'use strict'; " + code, module);
    }
    if (!no_fun) {
      test_reflect("(function() { " + code, module);
      if (!no_strict) {
        test_reflect("(function() { 'use strict'; " + code, module);
      }
    }

    if (!no_eval) {
      test_eval(code);
      if (!no_strict) {
        test_eval("'use strict'; " + code);
      }
      if (!no_fun) {
        test_eval("(function() { " + code);
        if (!no_strict) {
          test_eval("(function() { 'use strict'; " + code);
        }
      }
    }
  }

  function test_fun_arg(arg) {
    for (var postfix of postfixes) {
      var cur_arg = arg + postfix;

      var caught = false;
      try {
        new Function(cur_arg, "");
      } catch (e) {
        caught = true;
        check_error(e, cur_arg, "fun_arg");
      }
      assertEq(caught, true);
    }
  }

  // ==== Statements and declarations ====

  // ---- Control flow ----

  // Block

  test("{ ");
  test("{ } ");

  test("{ 1 ");
  test("{ 1; ");
  test("{ 1; } ");

  // break

  test("a: for (;;) { break ");
  test("a: for (;;) { break; ");
  test("a: for (;;) { break a ");
  test("a: for (;;) { break a; ");

  test("a: for (;;) { break\n");

  // continue

  test("a: for (;;) { continue ");
  test("a: for (;;) { continue; ");
  test("a: for (;;) { continue a ");
  test("a: for (;;) { continue a; ");

  test("a: for (;;) { continue\n");

  // Empty

  test("");
  test("; ");

  // if...else

  test("if ");
  test("if (");
  test("if (x ");
  test("if (x) ");
  test("if (x) { ");
  test("if (x) {} ");
  test("if (x) {} else ");
  test("if (x) {} else { ");
  test("if (x) {} else {} ");
  test("if (x) x ");
  test("if (x) x; ");
  test("if (x) x; else ");
  test("if (x) x; else y ");
  test("if (x) x; else y; ");

  // switch

  test("switch ");
  test("switch (");
  test("switch (x ");
  test("switch (x) ");
  test("switch (x) { ");
  test("switch (x) { case ");
  test("switch (x) { case 1 ");
  test("switch (x) { case 1: ");
  test("switch (x) { case 1: case ");
  test("switch (x) { case 1: case 2 ");
  test("switch (x) { case 1: case 2: ");
  test("switch (x) { case 1: case 2: x ");
  test("switch (x) { case 1: case 2: x; ");
  test("switch (x) { case 1: case 2: x; break ");
  test("switch (x) { case 1: case 2: x; break; ");
  test("switch (x) { case 1: case 2: x; break; case ");
  test("switch (x) { case 1: case 2: x; break; case 3 ");
  test("switch (x) { case 1: case 2: x; break; case 3: y ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; default ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; default: ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; default: z ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; default: z; ");
  test("switch (x) { case 1: case 2: x; break; case 3: y; default: z; } ");

  // throw

  test("throw ");
  test("throw x ");
  test("throw x; ");

  // try...catch

  test("try ");
  test("try { ");
  test("try {} ");
  test("try {} catch ");
  test("try {} catch ( ");
  test("try {} catch (e ");
  test("try {} catch (e) ");
  test("try {} catch (e) { ");
  test("try {} catch (e) {} ");
  test("try {} catch (e) {} finally ");
  test("try {} catch (e) {} finally { ");
  test("try {} catch (e) {} finally {} ");

  test("try {} catch (e if ");
  test("try {} catch (e if e  ");
  test("try {} catch (e if e instanceof ");
  test("try {} catch (e if e instanceof x ");
  test("try {} catch (e if e instanceof x) ");
  test("try {} catch (e if e instanceof x) { ");
  test("try {} catch (e if e instanceof x) {} ");

  // ---- Declarations ----

  // var

  test("var ");
  test("var x ");
  test("var x = ");
  test("var x = 1 ");
  test("var x = 1 + ");
  test("var x = 1 + 2 ");
  test("var x = 1 + 2, ");
  test("var x = 1 + 2, y ");
  test("var x = 1 + 2, y, ");
  test("var x = 1 + 2, y, z ");
  test("var x = 1 + 2, y, z; ");

  test("var [ ");
  test("var [ x ");
  test("var [ x, ");
  test("var [ x, ... ");
  test("var { ");
  test("var { x ");
  test("var { x: ");
  test("var { x: y ");
  test("var { x: y, ");
  test("var { x: y } ");
  test("var { x: y } = ");

  // let

  test("let ");
  test("let x ");
  test("let x = ");
  test("let x = 1 ");
  test("let x = 1 + ");
  test("let x = 1 + 2 ");
  test("let x = 1 + 2, ");
  test("let x = 1 + 2, y ");
  test("let x = 1 + 2, y, ");
  test("let x = 1 + 2, y, z ");
  test("let x = 1 + 2, y, z; ");

  test("let [ ");
  test("let [ x ");
  test("let [ x, ");
  test("let [ x, ... ");
  test("let { ");
  test("let { x ");
  test("let { x: ");
  test("let { x: y ");
  test("let { x: y, ");
  test("let { x: y } ");
  test("let { x: y } = ");

  // const

  test("const ");
  test("const x ");
  test("const x = ");
  test("const x = 1 ");
  test("const x = 1 + ");
  test("const x = 1 + 2 ");
  test("const x = 1 + 2, ");
  test("const x = 1 + 2, y = 0");
  test("const x = 1 + 2, y = 0, ");
  test("const x = 1 + 2, y = 0, z = 0 ");
  test("const x = 1 + 2, y = 0, z = 0; ");

  test("const [ ");
  test("const [ x ");
  test("const [ x, ");
  test("const [ x, ... ");
  test("const { ");
  test("const { x ");
  test("const { x: ");
  test("const { x: y ");
  test("const { x: y, ");
  test("const { x: y } ");
  test("const { x: y } = ");

  // ---- Functions ----

  // function

  test("function ");
  test("function f ");
  test("function f( ");
  test("function f(x ");
  test("function f(x, ");
  test("function f(x, [ ");
  test("function f(x, [y ");
  test("function f(x, [y, ");
  test("function f(x, [y, { ");
  test("function f(x, [y, {z ");
  test("function f(x, [y, {z: ");
  test("function f(x, [y, {z: zz ");
  test("function f(x, [y, {z: zz,  ");
  test("function f(x, [y, {z: zz, w ");
  test("function f(x, [y, {z: zz, w} ");
  test("function f(x, [y, {z: zz, w}] ");
  test("function f(x, [y, {z: zz, w}], ");
  test("function f(x, [y, {z: zz, w}], v ");
  test("function f(x, [y, {z: zz, w}], v= ");
  test("function f(x, [y, {z: zz, w}], v=1 ");
  test("function f(x, [y, {z: zz, w}], v=1, ");
  test("function f(x, [y, {z: zz, w}], v=1, ... ");
  test("function f(x, [y, {z: zz, w}], v=1, ...t ");
  test("function f(x, [y, {z: zz, w}], v=1, ...t) ");
  test("function f(x, [y, {z: zz, w}], v=1, ...t) {");
  test("function f(x, [y, {z: zz, w}], v=1, ...t) { x ");
  test("function f(x, [y, {z: zz, w}], v=1, ...t) { x; ");
  test("function f(x, [y, {z: zz, w}], v=1, ...t) { x; } ");

  // star function

  test("function* ");
  test("function* f ");
  test("function* f( ");
  test("function* f(x ");
  test("function* f(x, ");
  test("function* f(x, ... ");
  test("function* f(x, ...t ");
  test("function* f(x, ...t) ");
  test("function* f(x, ...t) {");
  test("function* f(x, ...t) { x ");
  test("function* f(x, ...t) { x; ");
  test("function* f(x, ...t) { x; } ");

  // return

  test("function f() { return ");
  test("function f() { return 1 ");
  test("function f() { return 1; ");
  test("function f() { return 1; } ");
  test("function f() { return; ");
  test("function f() { return\n");

  // yield

  test("function* f() { yield ");
  test("function* f() { yield 1 ");
  test("function* f() { yield* ");
  test("function* f() { yield* 1 ");

  test("function* f() { yield\n");
  test("function* f() { yield*\n");

  // ---- Iterations ----

  // do...while

  test("do ");
  test("do {");
  test("do {} ");
  test("do {} while ");
  test("do {} while ( ");
  test("do {} while (x ");
  test("do {} while (x) ");
  test("do {} while (x); ");

  test("do x ");
  test("do x; ");
  test("do x; while ");

  // for

  test("for ");
  test("for (");
  test("for (x ");
  test("for (x; ");
  test("for (x; y ");
  test("for (x; y; ");
  test("for (x; y; z ");
  test("for (x; y; z) ");
  test("for (x; y; z) { ");
  test("for (x; y; z) {} ");

  test("for (x; y; z) x ");
  test("for (x; y; z) x; ");

  test("for (var ");
  test("for (var x ");
  test("for (var x = ");
  test("for (var x = y ");
  test("for (var x = y; ");

  test("for (let ");
  test("for (let x ");
  test("for (let x = ");
  test("for (let x = y ");
  test("for (let x = y; ");

  // for...in

  test("for (x in ");
  test("for (x in y ");
  test("for (x in y) ");

  test("for (var x in ");
  test("for (var x in y ");
  test("for (var x in y) ");

  test("for (let x in ");
  test("for (let x in y ");
  test("for (let x in y) ");

  // for...of

  test("for (x of ");
  test("for (x of y ");
  test("for (x of y) ");

  test("for (var x of ");
  test("for (var x of y ");
  test("for (var x of y) ");

  test("for (let x of ");
  test("for (let x of y ");
  test("for (let x of y) ");

  // while

  test("while ");
  test("while (");
  test("while (x ");
  test("while (x) ");
  test("while (x) { ");
  test("while (x) {} ");

  test("while (x) x ");
  test("while (x) x; ");

  // ---- Others ----

  // debugger

  test("debugger ");
  test("debugger; ");

  // export

  var opts = { no_fun: true, no_eval: true, module: true };
  test("export ", opts);
  test("export { ", opts);
  test("export { x ", opts);
  test("export { x, ", opts);
  test("export { x, y ", opts);
  test("export { x, y as ", opts);
  test("export { x, y as z ", opts);
  test("export { x, y as z } ", opts);
  test("export { x, y as z } from ", opts);
  test("export { x, y as z } from 'a' ", opts);
  test("export { x, y as z } from 'a'; ", opts);

  test("export * ", opts);
  test("export * from ", opts);
  test("export * from 'a' ", opts);
  test("export * from 'a'; ", opts);

  test("export function ", opts);
  test("export function f ", opts);
  test("export function f( ", opts);
  test("export function f() ", opts);
  test("export function f() { ", opts);
  test("export function f() {} ", opts);
  test("export function f() {}; ", opts);

  test("export var ", opts);
  test("export var a ", opts);
  test("export var a = ", opts);
  test("export var a = 1 ", opts);
  test("export var a = 1, ", opts);
  test("export var a = 1, b ", opts);
  test("export var a = 1, b = ", opts);
  test("export var a = 1, b = 2 ", opts);
  test("export var a = 1, b = 2; ", opts);

  test("export let ", opts);
  test("export let a ", opts);
  test("export let a = ", opts);
  test("export let a = 1 ", opts);
  test("export let a = 1, ", opts);
  test("export let a = 1, b ", opts);
  test("export let a = 1, b = ", opts);
  test("export let a = 1, b = 2 ", opts);
  test("export let a = 1, b = 2; ", opts);

  test("export const ", opts);
  test("export const a ", opts);
  test("export const a = ", opts);
  test("export const a = 1 ", opts);
  test("export const a = 1, ", opts);
  test("export const a = 1, b ", opts);
  test("export const a = 1, b = ", opts);
  test("export const a = 1, b = 2 ", opts);
  test("export const a = 1, b = 2; ", opts);

  test("export class ", opts);
  test("export class Foo ", opts);
  test("export class Foo {  ", opts);
  test("export class Foo { constructor ", opts);
  test("export class Foo { constructor( ", opts);
  test("export class Foo { constructor() ", opts);
  test("export class Foo { constructor() { ", opts);
  test("export class Foo { constructor() {} ", opts);
  test("export class Foo { constructor() {} } ", opts);
  test("export class Foo { constructor() {} }; ", opts);

  test("export default ", opts);
  test("export default 1 ", opts);
  test("export default 1; ", opts);

  test("export default function ", opts);
  test("export default function() ", opts);
  test("export default function() { ", opts);
  test("export default function() {} ", opts);
  test("export default function() {}; ", opts);

  test("export default function foo ", opts);
  test("export default function foo( ", opts);
  test("export default function foo() ", opts);
  test("export default function foo() { ", opts);
  test("export default function foo() {} ", opts);
  test("export default function foo() {}; ", opts);

  test("export default class ", opts);
  test("export default class { ", opts);
  test("export default class { constructor ", opts);
  test("export default class { constructor( ", opts);
  test("export default class { constructor() ", opts);
  test("export default class { constructor() { ", opts);
  test("export default class { constructor() {} ", opts);
  test("export default class { constructor() {} } ", opts);
  test("export default class { constructor() {} }; ", opts);

  test("export default class Foo ", opts);
  test("export default class Foo { ", opts);
  test("export default class Foo { constructor ", opts);
  test("export default class Foo { constructor( ", opts);
  test("export default class Foo { constructor() ", opts);
  test("export default class Foo { constructor() { ", opts);
  test("export default class Foo { constructor() {} ", opts);
  test("export default class Foo { constructor() {} } ", opts);
  test("export default class Foo { constructor() {} }; ", opts);

  // import

  test("import ", opts);
  test("import x ", opts);
  test("import x from ", opts);
  test("import x from 'a' ", opts);
  test("import x from 'a'; ", opts);

  test("import { ", opts);
  test("import { x ", opts);
  test("import { x, ", opts);
  test("import { x, y ", opts);
  test("import { x, y } ", opts);
  test("import { x, y } from ", opts);
  test("import { x, y } from 'a' ", opts);
  test("import { x, y } from 'a'; ", opts);

  test("import { x as ", opts);
  test("import { x as y ", opts);
  test("import { x as y } ", opts);
  test("import { x as y } from ", opts);
  test("import { x as y } from 'a' ", opts);
  test("import { x as y } from 'a'; ", opts);

  test("import 'a' ", opts);
  test("import 'a'; ", opts);

  test("import * ", opts);
  test("import * as ", opts);
  test("import * as a ", opts);
  test("import * as a from ", opts);
  test("import * as a from 'a' ", opts);
  test("import * as a from 'a'; ", opts);

  test("import a ", opts);
  test("import a, ", opts);
  test("import a, * ", opts);
  test("import a, * as ", opts);
  test("import a, * as b ", opts);
  test("import a, * as b from ", opts);
  test("import a, * as b from 'c' ", opts);
  test("import a, * as b from 'c'; ", opts);

  test("import a, { ", opts);
  test("import a, { b ", opts);
  test("import a, { b } ", opts);
  test("import a, { b } from ", opts);
  test("import a, { b } from 'c' ", opts);
  test("import a, { b } from 'c'; ", opts);

  // label

  test("a ");
  test("a: ");

  // with

  opts = { no_strict: true };
  test("with ", opts);
  test("with (", opts);
  test("with (x ", opts);
  test("with (x) ", opts);
  test("with (x) { ", opts);
  test("with (x) {} ", opts);

  test("with (x) x ", opts);
  test("with (x) x; ", opts);

  // ==== Expressions and operators ====

  // ---- Primary expressions ----

  // this

  test("this ");

  // function

  test("(function ");
  test("(function ( ");
  test("(function (x ");
  test("(function (x, ");
  test("(function (x, ... ");
  test("(function (x, ...t ");
  test("(function (x, ...t) ");
  test("(function (x, ...t) {");
  test("(function (x, ...t) { x ");
  test("(function (x, ...t) { x; ");
  test("(function (x, ...t) { x; } ");
  test("(function (x, ...t) { x; }) ");

  // star function

  test("(function* ");
  test("(function* ( ");
  test("(function* (x ");
  test("(function* (x, ");
  test("(function* (x, ... ");
  test("(function* (x, ...t ");
  test("(function* (x, ...t) ");
  test("(function* (x, ...t) {");
  test("(function* (x, ...t) { x ");
  test("(function* (x, ...t) { x; ");
  test("(function* (x, ...t) { x; } ");
  test("(function* (x, ...t) { x; }) ");

  // Array literal

  test("[ ");
  test("[] ");
  test("[1 ");
  test("[1, ");
  test("[1, ... ");
  test("[1, ...x ");
  test("[1, ...x] ");

  // object

  test("({ ");
  test("({ x ");
  test("({ x: ");
  test("({ x: 1 ");
  test("({ x: 1, ");
  test("({ x: 1, y ");
  test("({ x: 1, y: ");
  test("({ x: 1, y: 2 ");
  test("({ x: 1, y: 2, ");
  test("({ x: 1, y: 2, z ");
  test("({ x: 1, y: 2, z, ");
  test("({ x: 1, y: 2, z, w ");
  test("({ x: 1, y: 2, z, w } ");
  test("({ x: 1, y: 2, z, w }) ");

  // object: computed property

  test("({ [");
  test("({ [k ");
  test("({ [k] ");
  test("({ [k]: ");
  test("({ [k]: 1 ");
  test("({ [k]: 1, ");

  // object: getter

  test("({ get ");
  test("({ get p ");
  test("({ get p( ");
  test("({ get p() ");
  test("({ get p() { ");
  test("({ get p() {} ");
  test("({ get p() {}, ");
  test("({ get p() {}, } ");

  test("({ get [ ");
  test("({ get [p ");
  test("({ get [p] ");
  test("({ get [p]( ");
  test("({ get [p]() ");

  // object: setter

  test("({ set ");
  test("({ set p ");
  test("({ set p( ");
  test("({ set p(v ");
  test("({ set p(v) ");
  test("({ set p(v) { ");
  test("({ set p(v) {} ");

  test("({ set [ ");
  test("({ set [p ");
  test("({ set [p] ");
  test("({ set [p]( ");
  test("({ set [p](v ");
  test("({ set [p](v) ");

  // object: method

  test("({ m ");
  test("({ m( ");
  test("({ m() ");
  test("({ m() { ");
  test("({ m() {} ");
  test("({ m() {}, ");

  test("({ [ ");
  test("({ [m ");
  test("({ [m] ");
  test("({ [m]( ");
  test("({ [m]() ");
  test("({ [m]() { ");
  test("({ [m]() {} ");
  test("({ [m]() {}, ");

  test("({ * ");
  test("({ *m ");
  test("({ *m( ");
  test("({ *m() ");
  test("({ *m() { ");
  test("({ *m() {} ");
  test("({ *m() {}, ");

  test("({ *[ ");
  test("({ *[m ");
  test("({ *[m] ");
  test("({ *[m]( ");
  test("({ *[m]() ");
  test("({ *[m]() { ");
  test("({ *[m]() {} ");
  test("({ *[m]() {}, ");

  test("({ * get ");
  test("({ * get ( ");
  test("({ * get () ");
  test("({ * get () { ");
  test("({ * get () {} ");
  test("({ * get () {}, ");

  test("({ * set ");
  test("({ * set ( ");
  test("({ * set () ");
  test("({ * set () { ");
  test("({ * set () {} ");
  test("({ * set () {}, ");

  // Regular expression literal

  test("/a/ ");
  test("/a/g ");

  // Array comprehensions

  test("[for ");
  test("[for ( ");
  test("[for (x ");
  test("[for (x of ");
  test("[for (x of y ");
  test("[for (x of y) ");
  test("[for (x of y) x ");
  test("[for (x of y) if ");
  test("[for (x of y) if ( ");
  test("[for (x of y) if (x ");
  test("[for (x of y) if (x == ");
  test("[for (x of y) if (x == 1 ");
  test("[for (x of y) if (x == 1) ");
  test("[for (x of y) if (x == 1) x ");
  test("[for (x of y) if (x == 1) x] ");

  // Generator comprehensions

  test("(for ");
  test("(for ( ");
  test("(for (x ");
  test("(for (x of ");
  test("(for (x of y ");
  test("(for (x of y) ");
  test("(for (x of y) x ");
  test("(for (x of y) if ");
  test("(for (x of y) if ( ");
  test("(for (x of y) if (x ");
  test("(for (x of y) if (x == ");
  test("(for (x of y) if (x == 1 ");
  test("(for (x of y) if (x == 1) ");
  test("(for (x of y) if (x == 1) x ");
  test("(for (x of y) if (x == 1) x) ");

  // ---- Left-hand-side expressions ----

  // property access

  test("a[ ");
  test("a[1 ");
  test("a[1] ");

  test("a. ");
  test("a.b ");
  test("a.b; ");

  // new

  test("new ");
  test("new f ");
  test("new f( ");
  test("new f() ");
  test("new f(); ");

  // ---- Increment and decrement ----

  test("a ++ ");
  test("a ++; ");

  test("-- ");
  test("-- a ");
  test("-- a; ");

  // ---- Unary operators ----

  // delete

  test("delete ");
  test("delete a ");
  test("delete a[ ");
  test("delete a[b ");
  test("delete a[b] ");
  test("delete a[b]; ");

  test("delete ( ");
  test("delete (a ");
  test("delete (a[ ");
  test("delete (a[b ");
  test("delete (a[b] ");
  test("delete (a[b]) ");
  test("delete (a[b]); ");

  // void

  test("void ");
  test("void a ");
  test("void a; ");

  test("void (");
  test("void (a ");
  test("void (a) ");
  test("void (a); ");

  // typeof

  test("typeof ");
  test("typeof a ");
  test("typeof a; ");

  test("typeof (");
  test("typeof (a ");
  test("typeof (a) ");
  test("typeof (a); ");

  // -

  test("- ");
  test("- 1 ");
  test("- 1; ");

  // +

  test("+ ");
  test("+ 1 ");
  test("+ 1; ");

  // ---- Arithmetic operators ----

  // +

  test("1 + ");
  test("1 + 1 ");
  test("1 + 1; ");

  // ---- Relational operators ----

  // in

  test("a in ");
  test("a in b ");
  test("a in b; ");

  // instanceof

  test("a instanceof ");
  test("a instanceof b ");
  test("a instanceof b; ");

  // ---- Equality operators ----

  // ==

  test("1 == ");
  test("1 == 1 ");
  test("1 == 1; ");

  // ---- Bitwise shift operators ----

  // <<

  test("1 << ");
  test("1 << 1 ");
  test("1 << 1; ");

  // ---- Binary bitwise operators ----

  // &

  test("1 & ");
  test("1 & 1 ");
  test("1 & 1; ");

  // ---- Binary logical operators ----

  // ||

  test("1 || ");
  test("1 || 1 ");
  test("1 || 1; ");

  // ---- Conditional (ternary) operator ----

  test("1 ? ");
  test("1 ? 2 ");
  test("1 ? 2 : ");
  test("1 ? 2 : 3 ");
  test("1 ? 2 : 3; ");

  // ---- Assignment operators ----

  test("x = ");
  test("x = 1 ");
  test("x = 1 + ");
  test("x = 1 + 2 ");
  test("x = 1 + 2; ");

  // ---- Comma operator ----

  test("1, ");
  test("1, 2 ");
  test("1, 2; ");

  // ---- Functions ----

  // Arrow functions

  test("a => ");
  test("a => 1 ");
  test("a => 1; ");
  test("a => { ");
  test("a => {} ");
  test("a => {}; ");

  test("( ");
  test("() ");
  test("() => ");

  test("(...");
  test("(...a ");
  test("(...a) ");
  test("(...a) => ");

  test("([ ");
  test("([a ");
  test("([a] ");
  test("([a]) ");
  test("([a]) => ");

  test("({ ");
  test("({a ");
  test("({a} ");
  test("({a}) ");
  test("({a}) => ");
  test("({a: ");
  test("({a: b ");
  test("({a: b, ");
  test("({a: b} ");
  test("({a: b}) ");
  test("({a: b}) => ");

  // ---- Class declaration ----

  test("class ");
  test("class a ");
  test("class a { ");
  test("class a { constructor ");
  test("class a { constructor( ");
  test("class a { constructor() ");
  test("class a { constructor() { ");
  test("class a { constructor() { } ");
  test("class a { constructor() { } } ");

  test("class a { constructor() { } static ");
  test("class a { constructor() { } static m ");
  test("class a { constructor() { } static m( ");
  test("class a { constructor() { } static m() ");
  test("class a { constructor() { } static m() { ");
  test("class a { constructor() { } static m() {} ");
  test("class a { constructor() { } static m() {} } ");

  test("class a { constructor() { } static ( ");
  test("class a { constructor() { } static () ");
  test("class a { constructor() { } static () { ");
  test("class a { constructor() { } static () {} ");
  test("class a { constructor() { } static () {} } ");

  test("class a { constructor() { } static get ");
  test("class a { constructor() { } static get p ");
  test("class a { constructor() { } static get p( ");
  test("class a { constructor() { } static get p() ");
  test("class a { constructor() { } static get p() { ");
  test("class a { constructor() { } static get p() {} ");
  test("class a { constructor() { } static get p() {} } ");

  test("class a { constructor() { } static set ");
  test("class a { constructor() { } static set p ");
  test("class a { constructor() { } static set p( ");
  test("class a { constructor() { } static set p(v ");
  test("class a { constructor() { } static set p(v) ");
  test("class a { constructor() { } static set p(v) { ");
  test("class a { constructor() { } static set p(v) {} ");
  test("class a { constructor() { } static set p(v) {} } ");

  test("class a { constructor() { } * ");
  test("class a { constructor() { } *m ");
  test("class a { constructor() { } *m( ");
  test("class a { constructor() { } *m() ");
  test("class a { constructor() { } *m() { ");
  test("class a { constructor() { } *m() {} ");
  test("class a { constructor() { } *m() {} } ");

  test("class a { constructor() { } static * ");
  test("class a { constructor() { } static *m ");
  test("class a { constructor() { } static *m( ");
  test("class a { constructor() { } static *m() ");
  test("class a { constructor() { } static *m() { ");
  test("class a { constructor() { } static *m() {} ");
  test("class a { constructor() { } static *m() {} } ");

  test("class a extends ");
  test("class a extends b ");
  test("class a extends b { ");

  test("class a extends ( ");
  test("class a extends ( b ");
  test("class a extends ( b ) ");
  test("class a extends ( b ) { ");

  // ---- Class expression ----

  test("( class ");
  test("( class a ");
  test("( class a { ");
  test("( class a { constructor ");
  test("( class a { constructor( ");
  test("( class a { constructor() ");
  test("( class a { constructor() { ");
  test("( class a { constructor() { } ");
  test("( class a { constructor() { } } ");
  test("( class a { constructor() { } } ) ");

  test("(class a extends ");
  test("(class a extends b ");
  test("(class a extends b { ");

  test("(class a extends ( ");
  test("(class a extends ( b ");
  test("(class a extends ( b ) ");
  test("(class a extends ( b ) { ");

  test("( class { ");
  test("( class { constructor ");
  test("( class { constructor( ");
  test("( class { constructor() ");
  test("( class { constructor() { ");
  test("( class { constructor() { } ");
  test("( class { constructor() { } } ");
  test("( class { constructor() { } } ) ");

  test("(class extends ");
  test("(class extends b ");
  test("(class extends b { ");

  test("(class extends ( ");
  test("(class extends ( b ");
  test("(class extends ( b ) ");
  test("(class extends ( b ) { ");

  // ---- Other ----

  // Literals

  test("a ");
  test("1 ");
  test("1. ");
  test("1.2 ");
  test("true ");
  test("false ");
  test("\"a\" ");
  test("'a' ");
  test("null ");

  // Template strings

  test("`${ ");
  test("`${a ");
  test("`${a}` ");

  // Function calls

  test("f( ");
  test("f() ");
  test("f(); ");

  test("f(... ");
  test("f(...x ");
  test("f(...x) ");

  // Function constructors

  test_fun_arg("");
  test_fun_arg("a ");
  test_fun_arg("... ");
  test_fun_arg("...a ");

  // ==== Legacy ====

  // Expression closures

  test("function f() 1 ");
  test("function f() 1; ");
  test("(function () 1 ");
  test("(function () 1); ");

  // Legacy generator

  test("function f() { (yield ");
  test("function f() { (yield 1 ");
  test("function f() { f(yield ");
  test("function f() { f(yield 1 ");

  // for each...in

  test("for each ");
  test("for each (");
  test("for each (x ");
  test("for each (x in ");
  test("for each (x in y ");
  test("for each (x in y) ");

  test("for each (var ");
  test("for each (var x ");
  test("for each (var x in ");
  test("for each (var x in y ");
  test("for each (var x in y) ");

  test("for each (let ");
  test("for each (let x ");
  test("for each (let x in ");
  test("for each (let x in y ");
  test("for each (let x in y) ");

  // asm.js

  test("(function() { 'use asm'; ");
  test("(function() { 'use asm'; var ");
  test("(function() { 'use asm'; var a ");
  test("(function() { 'use asm'; var a = ");
  test("(function() { 'use asm'; var a = 1 ");
  test("(function() { 'use asm'; var a = 1; ");
  test("(function() { 'use asm'; var a = 1; function ");
  test("(function() { 'use asm'; var a = 1; function f ");
  test("(function() { 'use asm'; var a = 1; function f( ");
  test("(function() { 'use asm'; var a = 1; function f() ");
  test("(function() { 'use asm'; var a = 1; function f() { ");
  test("(function() { 'use asm'; var a = 1; function f() { } ");
  test("(function() { 'use asm'; var a = 1; function f() { } var ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [ ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f] ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; } ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; }) ");
  test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; }); ");
}
