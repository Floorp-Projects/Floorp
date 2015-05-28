var JSMSG_ILLEGAL_CHARACTER = "illegal character";
var JSMSG_UNTERMINATED_STRING = "unterminated string literal";

function test_reflect(code) {
  var caught = false;
  try {
    Reflect.parse(code);
  } catch (e) {
    caught = true;
    assertEq(e instanceof SyntaxError, true, code);
    assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, code);
  }
  assertEq(caught, true);
}

function test_eval(code) {
  var caught = false;
  try {
    eval(code);
  } catch (e) {
    caught = true;
    assertEq(e instanceof SyntaxError, true, code);
    assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, code);
  }
  assertEq(caught, true);
}


function test(code) {
  test_reflect(code);
  test_reflect("'use strict'; " + code);
  test_reflect("(function() { " + code);
  test_reflect("(function() { 'use strict'; " + code);

  test_eval(code);
  test_eval("'use strict'; " + code);
  test_eval("(function() { " + code);
  test_eval("(function() { 'use strict'; " + code);
}

function test_no_strict(code) {
  test_reflect(code);
  test_reflect("(function() { " + code);

  test_eval(code);
  test_eval("(function() { " + code);
}

function test_no_fun_no_eval(code) {
  test_reflect(code);
  test_reflect("'use strict'; " + code);
}


function test_fun_arg(arg) {
  var caught = false;
  try {
    new Function(arg, "");
  } catch (e) {
    caught = true;
    assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, arg);
  }
  assertEq(caught, true);
}

// ==== Statements and declarations ====

// ---- Control flow ----

// Block

test("{ @");
test("{ } @");

test("{ 1 @");
test("{ 1; @");
test("{ 1; } @");

// break

test("a: for (;;) { break @");
test("a: for (;;) { break; @");
test("a: for (;;) { break a @");
test("a: for (;;) { break a; @");

test("a: for (;;) { break\n@");

// continue

test("a: for (;;) { continue @");
test("a: for (;;) { continue; @");
test("a: for (;;) { continue a @");
test("a: for (;;) { continue a; @");

test("a: for (;;) { continue\n@");

// Empty

test("@");
test("; @");

// if...else

test("if @");
test("if (@");
test("if (x @");
test("if (x) @");
test("if (x) { @");
test("if (x) {} @");
test("if (x) {} else @");
test("if (x) {} else { @");
test("if (x) {} else {} @");
test("if (x) x @");
test("if (x) x; @");
test("if (x) x; else @");
test("if (x) x; else y @");
test("if (x) x; else y; @");

// switch

test("switch @");
test("switch (@");
test("switch (x @");
test("switch (x) @");
test("switch (x) { @");
test("switch (x) { case @");
test("switch (x) { case 1 @");
test("switch (x) { case 1: @");
test("switch (x) { case 1: case @");
test("switch (x) { case 1: case 2 @");
test("switch (x) { case 1: case 2: @");
test("switch (x) { case 1: case 2: x @");
test("switch (x) { case 1: case 2: x; @");
test("switch (x) { case 1: case 2: x; break @");
test("switch (x) { case 1: case 2: x; break; @");
test("switch (x) { case 1: case 2: x; break; case @");
test("switch (x) { case 1: case 2: x; break; case 3 @");
test("switch (x) { case 1: case 2: x; break; case 3: y @");
test("switch (x) { case 1: case 2: x; break; case 3: y; @");
test("switch (x) { case 1: case 2: x; break; case 3: y; default @");
test("switch (x) { case 1: case 2: x; break; case 3: y; default: @");
test("switch (x) { case 1: case 2: x; break; case 3: y; default: z @");
test("switch (x) { case 1: case 2: x; break; case 3: y; default: z; @");
test("switch (x) { case 1: case 2: x; break; case 3: y; default: z; } @");

// throw

test("throw @");
test("throw x @");
test("throw x; @");

// try...catch

test("try @");
test("try { @");
test("try {} @");
test("try {} catch @");
test("try {} catch ( @");
test("try {} catch (e @");
test("try {} catch (e) @");
test("try {} catch (e) { @");
test("try {} catch (e) {} @");
test("try {} catch (e) {} finally @");
test("try {} catch (e) {} finally { @");
test("try {} catch (e) {} finally {} @");

test("try {} catch (e if @");
test("try {} catch (e if e  @");
test("try {} catch (e if e instanceof @");
test("try {} catch (e if e instanceof x @");
test("try {} catch (e if e instanceof x) @");
test("try {} catch (e if e instanceof x) { @");
test("try {} catch (e if e instanceof x) {} @");

// ---- Declarations ----

// var

test("var @");
test("var x @");
test("var x = @");
test("var x = 1 @");
test("var x = 1 + @");
test("var x = 1 + 2 @");
test("var x = 1 + 2, @");
test("var x = 1 + 2, y @");
test("var x = 1 + 2, y, @");
test("var x = 1 + 2, y, z @");
test("var x = 1 + 2, y, z; @");

test("var [ @");
test("var [ x @");
test("var [ x, @");
test("var [ x, ... @");
test("var { @");
test("var { x @");
test("var { x: @");
test("var { x: y @");
test("var { x: y, @");
test("var { x: y } @");
test("var { x: y } = @");

// let

test("let @");
test("let x @");
test("let x = @");
test("let x = 1 @");
test("let x = 1 + @");
test("let x = 1 + 2 @");
test("let x = 1 + 2, @");
test("let x = 1 + 2, y @");
test("let x = 1 + 2, y, @");
test("let x = 1 + 2, y, z @");
test("let x = 1 + 2, y, z; @");

test("let [ @");
test("let [ x @");
test("let [ x, @");
test("let [ x, ... @");
test("let { @");
test("let { x @");
test("let { x: @");
test("let { x: y @");
test("let { x: y, @");
test("let { x: y } @");
test("let { x: y } = @");

// const

test("const @");
test("const x @");
test("const x = @");
test("const x = 1 @");
test("const x = 1 + @");
test("const x = 1 + 2 @");
test("const x = 1 + 2, @");
test("const x = 1 + 2, y = 0@");
test("const x = 1 + 2, y = 0, @");
test("const x = 1 + 2, y = 0, z = 0 @");
test("const x = 1 + 2, y = 0, z = 0; @");

test("const [ @");
test("const [ x @");
test("const [ x, @");
test("const [ x, ... @");
test("const { @");
test("const { x @");
test("const { x: @");
test("const { x: y @");
test("const { x: y, @");
test("const { x: y } @");
test("const { x: y } = @");

// ---- Functions ----

// function

test("function @");
test("function f @");
test("function f( @");
test("function f(x @");
test("function f(x, @");
test("function f(x, [ @");
test("function f(x, [y @");
test("function f(x, [y, @");
test("function f(x, [y, { @");
test("function f(x, [y, {z @");
test("function f(x, [y, {z: @");
test("function f(x, [y, {z: zz @");
test("function f(x, [y, {z: zz,  @");
test("function f(x, [y, {z: zz, w @");
test("function f(x, [y, {z: zz, w} @");
test("function f(x, [y, {z: zz, w}] @");
test("function f(x, [y, {z: zz, w}], @");
test("function f(x, [y, {z: zz, w}], v @");
test("function f(x, [y, {z: zz, w}], v= @");
test("function f(x, [y, {z: zz, w}], v=1 @");
test("function f(x, [y, {z: zz, w}], v=1, @");
test("function f(x, [y, {z: zz, w}], v=1, ... @");
test("function f(x, [y, {z: zz, w}], v=1, ...t @");
test("function f(x, [y, {z: zz, w}], v=1, ...t) @");
test("function f(x, [y, {z: zz, w}], v=1, ...t) {@");
test("function f(x, [y, {z: zz, w}], v=1, ...t) { x @");
test("function f(x, [y, {z: zz, w}], v=1, ...t) { x; @");
test("function f(x, [y, {z: zz, w}], v=1, ...t) { x; } @");

// star function

test("function* @");
test("function* f @");
test("function* f( @");
test("function* f(x @");
test("function* f(x, @");
test("function* f(x, ... @");
test("function* f(x, ...t @");
test("function* f(x, ...t) @");
test("function* f(x, ...t) {@");
test("function* f(x, ...t) { x @");
test("function* f(x, ...t) { x; @");
test("function* f(x, ...t) { x; } @");

// return

test("function f() { return @");
test("function f() { return 1 @");
test("function f() { return 1; @");
test("function f() { return 1; } @");
test("function f() { return; @");
test("function f() { return\n@");

// yield

test("function* f() { yield @");
test("function* f() { yield 1 @");
test("function* f() { yield* @");
test("function* f() { yield* 1 @");

test("function* f() { yield\n@");
test("function* f() { yield*\n@");

// ---- Iterations ----

// do...while

test("do @");
test("do {@");
test("do {} @");
test("do {} while @");
test("do {} while ( @");
test("do {} while (x @");
test("do {} while (x) @");
test("do {} while (x); @");

test("do x @");
test("do x; @");
test("do x; while @");

// for

test("for @");
test("for (@");
test("for (x @");
test("for (x; @");
test("for (x; y @");
test("for (x; y; @");
test("for (x; y; z @");
test("for (x; y; z) @");
test("for (x; y; z) { @");
test("for (x; y; z) {} @");

test("for (x; y; z) x @");
test("for (x; y; z) x; @");

test("for (var @");
test("for (var x @");
test("for (var x = @");
test("for (var x = y @");
test("for (var x = y; @");

test("for (let @");
test("for (let x @");
test("for (let x = @");
test("for (let x = y @");
test("for (let x = y; @");

// for...in

test("for (x in @");
test("for (x in y @");
test("for (x in y) @");

test("for (var x in @");
test("for (var x in y @");
test("for (var x in y) @");

test("for (let x in @");
test("for (let x in y @");
test("for (let x in y) @");

// for...of

test("for (x of @");
test("for (x of y @");
test("for (x of y) @");

test("for (var x of @");
test("for (var x of y @");
test("for (var x of y) @");

test("for (let x of @");
test("for (let x of y @");
test("for (let x of y) @");

// while

test("while @");
test("while (@");
test("while (x @");
test("while (x) @");
test("while (x) { @");
test("while (x) {} @");

test("while (x) x @");
test("while (x) x; @");

// ---- Others ----

// debugger

test("debugger @");
test("debugger; @");

// export

test_no_fun_no_eval("export @");
test_no_fun_no_eval("export { @");
test_no_fun_no_eval("export { x @");
test_no_fun_no_eval("export { x, @");
test_no_fun_no_eval("export { x, y @");
test_no_fun_no_eval("export { x, y as @");
test_no_fun_no_eval("export { x, y as z @");
test_no_fun_no_eval("export { x, y as z } @");
test_no_fun_no_eval("export { x, y as z } from @");
test_no_fun_no_eval("export { x, y as z } from 'a' @");
test_no_fun_no_eval("export { x, y as z } from 'a'; @");

test_no_fun_no_eval("export * @");
test_no_fun_no_eval("export * from @");
test_no_fun_no_eval("export * from 'a' @");
test_no_fun_no_eval("export * from 'a'; @");

test_no_fun_no_eval("export function @");
test_no_fun_no_eval("export function f @");
test_no_fun_no_eval("export function f( @");
test_no_fun_no_eval("export function f() @");
test_no_fun_no_eval("export function f() { @");
test_no_fun_no_eval("export function f() {} @");
test_no_fun_no_eval("export function f() {}; @");

test_no_fun_no_eval("export var @");
test_no_fun_no_eval("export var a @");
test_no_fun_no_eval("export var a = @");
test_no_fun_no_eval("export var a = 1 @");
test_no_fun_no_eval("export var a = 1, @");
test_no_fun_no_eval("export var a = 1, b @");
test_no_fun_no_eval("export var a = 1, b = @");
test_no_fun_no_eval("export var a = 1, b = 2 @");
test_no_fun_no_eval("export var a = 1, b = 2; @");

test_no_fun_no_eval("export let @");
test_no_fun_no_eval("export let a @");
test_no_fun_no_eval("export let a = @");
test_no_fun_no_eval("export let a = 1 @");
test_no_fun_no_eval("export let a = 1, @");
test_no_fun_no_eval("export let a = 1, b @");
test_no_fun_no_eval("export let a = 1, b = @");
test_no_fun_no_eval("export let a = 1, b = 2 @");
test_no_fun_no_eval("export let a = 1, b = 2; @");

test_no_fun_no_eval("export const @");
test_no_fun_no_eval("export const a @");
test_no_fun_no_eval("export const a = @");
test_no_fun_no_eval("export const a = 1 @");
test_no_fun_no_eval("export const a = 1, @");
test_no_fun_no_eval("export const a = 1, b @");
test_no_fun_no_eval("export const a = 1, b = @");
test_no_fun_no_eval("export const a = 1, b = 2 @");
test_no_fun_no_eval("export const a = 1, b = 2; @");

test_no_fun_no_eval("export class @");
test_no_fun_no_eval("export class Foo @");
test_no_fun_no_eval("export class Foo {  @");
test_no_fun_no_eval("export class Foo { constructor @");
test_no_fun_no_eval("export class Foo { constructor( @");
test_no_fun_no_eval("export class Foo { constructor() @");
test_no_fun_no_eval("export class Foo { constructor() { @");
test_no_fun_no_eval("export class Foo { constructor() {} @");
test_no_fun_no_eval("export class Foo { constructor() {} } @");
test_no_fun_no_eval("export class Foo { constructor() {} }; @");

test_no_fun_no_eval("export default @");
test_no_fun_no_eval("export default 1 @");
test_no_fun_no_eval("export default 1; @");

test_no_fun_no_eval("export default function @");
test_no_fun_no_eval("export default function() @");
test_no_fun_no_eval("export default function() { @");
test_no_fun_no_eval("export default function() {} @");
test_no_fun_no_eval("export default function() {}; @");

test_no_fun_no_eval("export default function foo @");
test_no_fun_no_eval("export default function foo( @");
test_no_fun_no_eval("export default function foo() @");
test_no_fun_no_eval("export default function foo() { @");
test_no_fun_no_eval("export default function foo() {} @");
test_no_fun_no_eval("export default function foo() {}; @");

test_no_fun_no_eval("export default class @");
test_no_fun_no_eval("export default class { @");
test_no_fun_no_eval("export default class { constructor @");
test_no_fun_no_eval("export default class { constructor( @");
test_no_fun_no_eval("export default class { constructor() @");
test_no_fun_no_eval("export default class { constructor() { @");
test_no_fun_no_eval("export default class { constructor() {} @");
test_no_fun_no_eval("export default class { constructor() {} } @");
test_no_fun_no_eval("export default class { constructor() {} }; @");

test_no_fun_no_eval("export default class Foo @");
test_no_fun_no_eval("export default class Foo { @");
test_no_fun_no_eval("export default class Foo { constructor @");
test_no_fun_no_eval("export default class Foo { constructor( @");
test_no_fun_no_eval("export default class Foo { constructor() @");
test_no_fun_no_eval("export default class Foo { constructor() { @");
test_no_fun_no_eval("export default class Foo { constructor() {} @");
test_no_fun_no_eval("export default class Foo { constructor() {} } @");
test_no_fun_no_eval("export default class Foo { constructor() {} }; @");

// import

test_no_fun_no_eval("import @");
test_no_fun_no_eval("import x @");
test_no_fun_no_eval("import x from @");
test_no_fun_no_eval("import x from 'a' @");
test_no_fun_no_eval("import x from 'a'; @");

test_no_fun_no_eval("import { @");
test_no_fun_no_eval("import { x @");
test_no_fun_no_eval("import { x, @");
test_no_fun_no_eval("import { x, y @");
test_no_fun_no_eval("import { x, y } @");
test_no_fun_no_eval("import { x, y } from @");
test_no_fun_no_eval("import { x, y } from 'a' @");
test_no_fun_no_eval("import { x, y } from 'a'; @");

test_no_fun_no_eval("import { x as @");
test_no_fun_no_eval("import { x as y @");
test_no_fun_no_eval("import { x as y } @");
test_no_fun_no_eval("import { x as y } from @");
test_no_fun_no_eval("import { x as y } from 'a' @");
test_no_fun_no_eval("import { x as y } from 'a'; @");

test_no_fun_no_eval("import 'a' @");
test_no_fun_no_eval("import 'a'; @");

test_no_fun_no_eval("import * @");
test_no_fun_no_eval("import * as @");
test_no_fun_no_eval("import * as a @");
test_no_fun_no_eval("import * as a from @");
test_no_fun_no_eval("import * as a from 'a' @");
test_no_fun_no_eval("import * as a from 'a'; @");

test_no_fun_no_eval("import a @");
test_no_fun_no_eval("import a, @");
test_no_fun_no_eval("import a, * @");
test_no_fun_no_eval("import a, * as @");
test_no_fun_no_eval("import a, * as b @");
test_no_fun_no_eval("import a, * as b from @");
test_no_fun_no_eval("import a, * as b from 'c' @");
test_no_fun_no_eval("import a, * as b from 'c'; @");

test_no_fun_no_eval("import a, { @");
test_no_fun_no_eval("import a, { b @");
test_no_fun_no_eval("import a, { b } @");
test_no_fun_no_eval("import a, { b } from @");
test_no_fun_no_eval("import a, { b } from 'c' @");
test_no_fun_no_eval("import a, { b } from 'c'; @");

// label

test("a @");
test("a: @");

// with

test_no_strict("with @");
test_no_strict("with (@");
test_no_strict("with (x @");
test_no_strict("with (x) @");
test_no_strict("with (x) { @");
test_no_strict("with (x) {} @");

test_no_strict("with (x) x @");
test_no_strict("with (x) x; @");

// ==== Expressions and operators ====

// ---- Primary expressions ----

// this

test("this @");

// function

test("(function @");
test("(function ( @");
test("(function (x @");
test("(function (x, @");
test("(function (x, ... @");
test("(function (x, ...t @");
test("(function (x, ...t) @");
test("(function (x, ...t) {@");
test("(function (x, ...t) { x @");
test("(function (x, ...t) { x; @");
test("(function (x, ...t) { x; } @");
test("(function (x, ...t) { x; }) @");

// star function

test("(function* @");
test("(function* ( @");
test("(function* (x @");
test("(function* (x, @");
test("(function* (x, ... @");
test("(function* (x, ...t @");
test("(function* (x, ...t) @");
test("(function* (x, ...t) {@");
test("(function* (x, ...t) { x @");
test("(function* (x, ...t) { x; @");
test("(function* (x, ...t) { x; } @");
test("(function* (x, ...t) { x; }) @");

// Array literal

test("[ @");
test("[] @");
test("[1 @");
test("[1, @");
test("[1, ... @");
test("[1, ...x @");
test("[1, ...x] @");

// object

test("({ @");
test("({ x @");
test("({ x: @");
test("({ x: 1 @");
test("({ x: 1, @");
test("({ x: 1, y @");
test("({ x: 1, y: @");
test("({ x: 1, y: 2 @");
test("({ x: 1, y: 2, @");
test("({ x: 1, y: 2, z @");
test("({ x: 1, y: 2, z, @");
test("({ x: 1, y: 2, z, w @");
test("({ x: 1, y: 2, z, w } @");
test("({ x: 1, y: 2, z, w }) @");

// object: computed property

test("({ [@");
test("({ [k @");
test("({ [k] @");
test("({ [k]: @");
test("({ [k]: 1 @");
test("({ [k]: 1, @");

// object: getter

test("({ get @");
test("({ get p @");
test("({ get p( @");
test("({ get p() @");
test("({ get p() { @");
test("({ get p() {} @");
test("({ get p() {}, @");
test("({ get p() {}, } @");

test("({ get [ @");
test("({ get [p @");
test("({ get [p] @");
test("({ get [p]( @");
test("({ get [p]() @");

// object: setter

test("({ set @");
test("({ set p @");
test("({ set p( @");
test("({ set p(v @");
test("({ set p(v) @");
test("({ set p(v) { @");
test("({ set p(v) {} @");

test("({ set [ @");
test("({ set [p @");
test("({ set [p] @");
test("({ set [p]( @");
test("({ set [p](v @");
test("({ set [p](v) @");

// object: method

test("({ m @");
test("({ m( @");
test("({ m() @");
test("({ m() { @");
test("({ m() {} @");
test("({ m() {}, @");

test("({ [ @");
test("({ [m @");
test("({ [m] @");
test("({ [m]( @");
test("({ [m]() @");
test("({ [m]() { @");
test("({ [m]() {} @");
test("({ [m]() {}, @");

test("({ * @");
test("({ *m @");
test("({ *m( @");
test("({ *m() @");
test("({ *m() { @");
test("({ *m() {} @");
test("({ *m() {}, @");

test("({ *[ @");
test("({ *[m @");
test("({ *[m] @");
test("({ *[m]( @");
test("({ *[m]() @");
test("({ *[m]() { @");
test("({ *[m]() {} @");
test("({ *[m]() {}, @");

test("({ * get @");
test("({ * get ( @");
test("({ * get () @");
test("({ * get () { @");
test("({ * get () {} @");
test("({ * get () {}, @");

test("({ * set @");
test("({ * set ( @");
test("({ * set () @");
test("({ * set () { @");
test("({ * set () {} @");
test("({ * set () {}, @");

// Regular expression literal

test("/a/ @");
test("/a/g @");

// Array comprehensions

test("[for @");
test("[for ( @");
test("[for (x @");
test("[for (x of @");
test("[for (x of y @");
test("[for (x of y) @");
test("[for (x of y) x @");
test("[for (x of y) if @");
test("[for (x of y) if ( @");
test("[for (x of y) if (x @");
test("[for (x of y) if (x == @");
test("[for (x of y) if (x == 1 @");
test("[for (x of y) if (x == 1) @");
test("[for (x of y) if (x == 1) x @");
test("[for (x of y) if (x == 1) x] @");

// Generator comprehensions

test("(for @");
test("(for ( @");
test("(for (x @");
test("(for (x of @");
test("(for (x of y @");
test("(for (x of y) @");
test("(for (x of y) x @");
test("(for (x of y) if @");
test("(for (x of y) if ( @");
test("(for (x of y) if (x @");
test("(for (x of y) if (x == @");
test("(for (x of y) if (x == 1 @");
test("(for (x of y) if (x == 1) @");
test("(for (x of y) if (x == 1) x @");
test("(for (x of y) if (x == 1) x) @");

// ---- Left-hand-side expressions ----

// property access

test("a[ @");
test("a[1 @");
test("a[1] @");

test("a. @");
test("a.b @");
test("a.b; @");

// new

test("new @");
test("new f @");
test("new f( @");
test("new f() @");
test("new f(); @");

// ---- Increment and decrement ----

test("a ++ @");
test("a ++; @");

test("-- @");
test("-- a @");
test("-- a; @");

// ---- Unary operators ----

// delete

test("delete @");
test("delete a @");
test("delete a[ @");
test("delete a[b @");
test("delete a[b] @");
test("delete a[b]; @");

test("delete ( @");
test("delete (a @");
test("delete (a[ @");
test("delete (a[b @");
test("delete (a[b] @");
test("delete (a[b]) @");
test("delete (a[b]); @");

// void

test("void @");
test("void a @");
test("void a; @");

test("void (@");
test("void (a @");
test("void (a) @");
test("void (a); @");

// typeof

test("typeof @");
test("typeof a @");
test("typeof a; @");

test("typeof (@");
test("typeof (a @");
test("typeof (a) @");
test("typeof (a); @");

// -

test("- @");
test("- 1 @");
test("- 1; @");

// +

test("+ @");
test("+ 1 @");
test("+ 1; @");

// ---- Arithmetic operators ----

// +

test("1 + @");
test("1 + 1 @");
test("1 + 1; @");

// ---- Relational operators ----

// in

test("a in @");
test("a in b @");
test("a in b; @");

// instanceof

test("a instanceof @");
test("a instanceof b @");
test("a instanceof b; @");

// ---- Equality operators ----

// ==

test("1 == @");
test("1 == 1 @");
test("1 == 1; @");

// ---- Bitwise shift operators ----

// <<

test("1 << @");
test("1 << 1 @");
test("1 << 1; @");

// ---- Binary bitwise operators ----

// &

test("1 & @");
test("1 & 1 @");
test("1 & 1; @");

// ---- Binary logical operators ----

// ||

test("1 || @");
test("1 || 1 @");
test("1 || 1; @");

// ---- Conditional (ternary) operator ----

test("1 ? @");
test("1 ? 2 @");
test("1 ? 2 : @");
test("1 ? 2 : 3 @");
test("1 ? 2 : 3; @");

// ---- Assignment operators ----

test("x = @");
test("x = 1 @");
test("x = 1 + @");
test("x = 1 + 2 @");
test("x = 1 + 2; @");

// ---- Comma operator ----

test("1, @");
test("1, 2 @");
test("1, 2; @");

// ---- Functions ----

// Arrow functions

test("a => @");
test("a => 1 @");
test("a => 1; @");
test("a => { @");
test("a => {} @");
test("a => {}; @");

test("( @");
test("() @");
test("() => @");

test("(...@");
test("(...a @");
test("(...a) @");
test("(...a) => @");

test("([ @");
test("([a @");
test("([a] @");
test("([a]) @");
test("([a]) => @");

test("({ @");
test("({a @");
test("({a} @");
test("({a}) @");
test("({a}) => @");
test("({a: @");
test("({a: b @");
test("({a: b, @");
test("({a: b} @");
test("({a: b}) @");
test("({a: b}) => @");

// ---- Other ----

// Literals

test("a @");
test("1 @");
test("1. @");
test("1.2 @");
test("true @");
test("false @");
test("\"a\" @");
test("'a' @");
test("null @");

// Template strings

test("`${ @");
test("`${a @");
test("`${a}` @");

// Unterminated Template strings
// This should not be treated as Illegal character error.

var caught = false;
try {
  Reflect.parse("`${1}@");
} catch (e) {
  caught = true;
  assertEq(e.message, JSMSG_UNTERMINATED_STRING);
}
assertEq(caught, true);

// Function calls

test("f( @");
test("f() @");
test("f(); @");

test("f(... @");
test("f(...x @");
test("f(...x) @");

// Function constructors

test_fun_arg("@");
test_fun_arg("a @");
test_fun_arg("... @");
test_fun_arg("...a @");

// ==== Legacy ====

// let statement

test("let ( @");
test("let ( x @");
test("let ( x = @");
test("let ( x = 1 @");
test("let ( x = 1, @");
test("let ( x = 1, y @");
test("let ( x = 1, y = @");
test("let ( x = 1, y = 2 @");
test("let ( x = 1, y = 2 ) @");
test("let ( x = 1, y = 2 ) { @");
test("let ( x = 1, y = 2 ) { x @");
test("let ( x = 1, y = 2 ) { x; @");
test("let ( x = 1, y = 2 ) { x; } @");

// Expression closures

test("function f() 1 @");
test("function f() 1; @");
test("(function () 1 @");
test("(function () 1); @");

// Legacy generator

test("function f() { (yield @");
test("function f() { (yield 1 @");
test("function f() { f(yield @");
test("function f() { f(yield 1 @");

// for each...in

test("for each @");
test("for each (@");
test("for each (x @");
test("for each (x in @");
test("for each (x in y @");
test("for each (x in y) @");

test("for each (var @");
test("for each (var x @");
test("for each (var x in @");
test("for each (var x in y @");
test("for each (var x in y) @");

test("for each (let @");
test("for each (let x @");
test("for each (let x in @");
test("for each (let x in y @");
test("for each (let x in y) @");

// Legacy array comprehensions

test("[x @");
test("[x for @");
test("[x for ( @");
test("[x for (x @");
test("[x for (x of @");
test("[x for (x of y @");
test("[x for (x of y) @");
test("[x for (x of y) if @");
test("[x for (x of y) if ( @");
test("[x for (x of y) if (x @");
test("[x for (x of y) if (x == @");
test("[x for (x of y) if (x == 1 @");
test("[x for (x of y) if (x == 1) @");
test("[x for (x of y) if (x == 1)] @");

test("[x for (x in @");
test("[x for (x in y @");
test("[x for (x in y) @");

test("[x for each @");
test("[x for each ( @");
test("[x for each (x @");
test("[x for each (x in @");
test("[x for each (x in y @");
test("[x for each (x in y) @");

// Generator expressions

test("(x @");
test("(x for @");
test("(x for ( @");
test("(x for (x @");
test("(x for (x of @");
test("(x for (x of y @");
test("(x for (x of y) @");
test("(x for (x of y) if @");
test("(x for (x of y) if ( @");
test("(x for (x of y) if (x @");
test("(x for (x of y) if (x == @");
test("(x for (x of y) if (x == 1 @");
test("(x for (x of y) if (x == 1) @");
test("(x for (x of y) if (x == 1)) @");

test("(x for (x in @");
test("(x for (x in y @");
test("(x for (x in y) @");

test("(x for each @");
test("(x for each ( @");
test("(x for each (x @");
test("(x for each (x in @");
test("(x for each (x in y @");
test("(x for each (x in y) @");
