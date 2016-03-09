/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// This file tests contextual restrictions for yield and arguments, and is
// derived from js1_8/genexps/regress-634472.js.

function error(str) {
  var base;
  try {
    // the following line must not be broken up into multiple lines
    base = (function(){try{eval('throw new Error()')}catch(e){return e.lineNumber}})(); eval(str);
    return null;
  } catch (e) {
    e.lineNumber = e.lineNumber - base + 1;
    return e;
  }
}

const YIELD_PAREN          = error("(function*(){(for (y of (yield 1, 2)) y)})").message;
const GENEXP_YIELD         = error("(function*(){(for (x of yield 1) x)})").message;
const TOP_YIELD            = error("yield").message;
const GENERIC              = error("(for)").message;
const BAD_GENERATOR_SYNTAX = error("(for (x of []) x, 1)").message;
const MISSING_SEMI         = error("yield 1").message;
const PAREN_PAREN          = error("(foo").message;
const FOR_OF_PAREN         = error("(for (x of y, z) w)").message;

const cases = [
// Expressions involving yield without a value, not currently implemented.  Many
// of these errors would need to be updated.  (Note: line numbers below might be
// mere placeholders, not actually the expected correct behavior -- check before
// blindly uncommenting.)
//{ expr: "yield",        top: [TOP_YIELD, 777], fun: null,              gen: [GENEXP_YIELD, 2], desc: "simple yield" },
//{ expr: "1, yield",     top: [TOP_YIELD, 777], fun: null,              gen: [GENEXP_YIELD, 2], desc: "simple yield at end of list" },
//{ expr: "yield, 1",     top: [TOP_YIELD, 777], fun: [YIELD_PAREN, 2], gen: [YIELD_PAREN, 2],  desc: "simple yield in list" },
//{ expr: "(yield)",      top: [TOP_YIELD, 777], fun: null,              gen: [GENEXP_YIELD, 2], desc: "simple yield, parenthesized" },
//{ expr: "(1, yield)",   top: [TOP_YIELD, 777], fun: null,              gen: [GENEXP_YIELD, 2], desc: "simple yield at end of list, parenthesized" },
//{ expr: "(yield, 1)",   top: [TOP_YIELD, 777], fun: [YIELD_PAREN, 2], gen: [YIELD_PAREN, 2],  desc: "simple yield in list, parenthesized" },
//{ expr: "((((yield))))",   top: [TOP_YIELD, 777], fun: null, gen: [GENEXP_YIELD, 2], desc: "deeply nested yield" },
//{ expr: "(for (x of []) yield)",           top: [TOP_YIELD, 777], fun: [GENERIC, 777],      gen: [GENERIC, 777],      desc: "simple yield in genexp" },
//{ expr: "(for (x of []) yield, 1)",        top: [TOP_YIELD, 777], fun: [YIELD_PAREN, 2],  gen: [YIELD_PAREN, 2],  desc: "simple yield in list in genexp" },
//{ expr: "(for (x of []) 1, yield)",        top: [TOP_YIELD, 777], fun: [GENERIC, 777],      gen: [GENERIC, 777],      desc: "simple yield at end of list in genexp" },
//{ expr: "(for (x of []) (yield))",         top: [TOP_YIELD, 777], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], desc: "simple yield, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (yield))",      top: [TOP_YIELD, 777], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], desc: "simple yield, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (1, yield))",      top: [TOP_YIELD, 777], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], desc: "simple yield at end of list, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (2, yield))",   top: [TOP_YIELD, 777], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], desc: "simple yield at end of list, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (yield, 1))",      top: [TOP_YIELD, 777], fun: [YIELD_PAREN, 2],  gen: [YIELD_PAREN, 2],  desc: "simple yield in list, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (yield, 2))",   top: [TOP_YIELD, 777], fun: [YIELD_PAREN, 2],  gen: [YIELD_PAREN, 2],  desc: "simple yield in list, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (function*() { yield }))",           top: null, fun: null, gen: null, desc: "legal yield in nested function" },

  // yield expressions
  { expr: "yield 1",      top: [MISSING_SEMI, 2], fun: [MISSING_SEMI, 2], gen: null, genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg" },
  { expr: "1, yield 2",   top: [MISSING_SEMI, 2], fun: [MISSING_SEMI, 2], gen: null, genexp: [FOR_OF_PAREN, 1], desc: "yield w/ arg at end of list" },
  { expr: "yield 1, 2",   top: [MISSING_SEMI, 2], fun: [MISSING_SEMI, 2], gen: null, genexp: [FOR_OF_PAREN, 3], desc: "yield w/ arg in list" },
  { expr: "(yield 1)",    top: [PAREN_PAREN, 2], fun:  [PAREN_PAREN, 2], gen: null, genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg, parenthesized" },
  { expr: "(1, yield 2)", top: [PAREN_PAREN, 2], fun:  [PAREN_PAREN, 2], gen: null, genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg at end of list, parenthesized" },
  { expr: "(yield 1, 2)", top: [PAREN_PAREN, 2], fun:  [PAREN_PAREN, 2], gen: null, genexp: [YIELD_PAREN, 2], desc: "yield w/ arg in list, parenthesized" },

  // deeply nested yield expressions
  { expr: "((((yield 1))))", top: [PAREN_PAREN, 2], fun: [PAREN_PAREN, 2], gen: null, genexp: [GENEXP_YIELD, 2], desc: "deeply nested yield w/ arg" },

  // arguments
  { expr: "arguments",    top: null, fun: null, gen: null, genexp: null, desc: "arguments in list" },
  { expr: "1, arguments", top: null, fun: null, gen: null, genexp: [FOR_OF_PAREN, 1], desc: "arguments in list" },

  // yield in generator expressions
  { expr: "(for (x of []) yield 1)",         top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg in genexp" },
  { expr: "(for (x of []) yield 1, 2)",      top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2],  gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg in list in genexp" },
  { expr: "(for (x of []) 1, yield 2)",      top: [PAREN_PAREN, 1], fun: [PAREN_PAREN, 1],  gen: [PAREN_PAREN, 1], genexp: [PAREN_PAREN, 1], desc: "yield w/ arg at end of list in genexp" },
  { expr: "(for (x of []) (yield 1))",       top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (yield 2))",    top: [PAREN_PAREN, 1], fun: [PAREN_PAREN, 1], gen: [PAREN_PAREN, 1], genexp: [PAREN_PAREN, 1], desc: "yield w/ arg, parenthesized in list in genexp" },
  { expr: "(for (x of []) (1, yield 2))",    top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "yield w/ arg at end of list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (2, yield 3))", top: [PAREN_PAREN, 1], fun: [PAREN_PAREN, 1], gen: [PAREN_PAREN, 1], genexp: [PAREN_PAREN, 1], desc: "yield w/ arg at end of list, parenthesized in list in genexp" },
  { expr: "(for (x of []) (yield 1, 2))",    top: [YIELD_PAREN, 2], fun: [YIELD_PAREN, 2], gen: [YIELD_PAREN, 2], genexp: [YIELD_PAREN, 2], desc: "yield w/ arg in list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (yield 2, 3))", top: [PAREN_PAREN, 1], fun: [PAREN_PAREN, 1], gen: [PAREN_PAREN, 1], genexp: [PAREN_PAREN, 1], desc: "yield w/ arg in list, parenthesized in list in genexp" },

  // deeply nested yield in generator expressions
  { expr: "(for (x of []) (((1, yield 2))))",               top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "deeply nested yield in genexp" },
  { expr: "(for (y of []) (for (x of []) ((1, yield 2))))", top: [GENEXP_YIELD, 2], fun: [GENEXP_YIELD, 2], gen: [GENEXP_YIELD, 2], genexp: [GENEXP_YIELD, 2], desc: "deeply nested yield in multiple genexps" },

  // arguments in generator expressions
  { expr: "(for (x of []) arguments)",         top: null, fun: null, gen: null, genexp: null, desc: "simple arguments in genexp" },
  { expr: "(for (x of []) 1, arguments)",      top: [BAD_GENERATOR_SYNTAX, 1], fun: [BAD_GENERATOR_SYNTAX, 1], gen: [BAD_GENERATOR_SYNTAX, 1], genexp: [BAD_GENERATOR_SYNTAX, 1], desc: "arguments in list in genexp" },
  { expr: "(for (x of []) (arguments))",       top: null, fun: null, gen: null, genexp: null, desc: "arguments, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (arguments))",    top: [BAD_GENERATOR_SYNTAX, 1], fun: [BAD_GENERATOR_SYNTAX, 1], gen: [BAD_GENERATOR_SYNTAX, 1], genexp: [BAD_GENERATOR_SYNTAX, 1], desc: "arguments, parenthesized in list in genexp" },
  { expr: "(for (x of []) (1, arguments))",    top: null, fun: null, gen: null, genexp: null, desc: "arguments in list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (2, arguments))", top: [BAD_GENERATOR_SYNTAX, 1], fun: [BAD_GENERATOR_SYNTAX, 1], gen: [BAD_GENERATOR_SYNTAX, 1], genexp: [BAD_GENERATOR_SYNTAX, 1], desc: "arguments in list, parenthesized in list in genexp" },

  // deeply nested arguments in generator expressions
  { expr: "(for (x of []) (((1, arguments))))",               top: null, fun: null, gen: null, genexp: null, desc: "deeply nested arguments in genexp" },
  { expr: "(for (y of []) (for (x of []) ((1, arguments))))", top: null, fun: null, gen: null, genexp: null, desc: "deeply nested arguments in multiple genexps" },

  // legal yield/arguments in nested function
  { expr: "(for (x of []) (function*() { yield 1 }))",         top: null, fun: null, gen: null, genexp: null, desc: "legal yield in nested function" },
  { expr: "(for (x of []) (function() { arguments }))",       top: null, fun: null, gen: null, genexp: null, desc: "legal arguments in nested function" },
  { expr: "(for (x of []) (function() arguments))",           top: null, fun: null, gen: null, genexp: null, desc: "legal arguments in nested expression-closure" }
];

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function splitKeyword(str) {
  return str.
//         replace(/[)] yield/, ')\nyield\n').
         replace(/yield ([0-9])/, '\nyield $1\n').
         replace(/yield([^ ]|$)/, '\nyield\n$1').
         replace(/arguments/, '\narguments\n');
}

function expectError1(err, ctx, msg, lineNumber) {
  reportCompare('object', typeof err,     'exn for: ' + msg);
  reportCompare(ctx,      err.message,    'exn message for: ' + msg);
  reportCompare(lineNumber,    err.lineNumber, 'exn token for: ' + msg);
}

function expectError(expr, wrapCtx, [expect, lineNumber], msg) {
  expectError1(error(wrapCtx(expr)), expect, msg, lineNumber);
}

function expectSuccess(err, msg) {
  reportCompare(null, err, 'parse: ' + msg);
}

function atTop(str) { return str }
function inFun(str) { return '(function(){' + str + '})' }
function inGen(str) { return '(function*(){' + str + '})' }
function inGenExp(str) { return '(for (y of ' + str + ') y)' }

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0, len = cases.length; i < len; i++) {
    var expr, top, fun, gen, genexp, desc;
    expr = cases[i].expr;
    top = cases[i].top;
    fun = cases[i].fun;
    gen = cases[i].gen;
    genexp = cases[i].genexp;
    desc = cases[i].desc;

    expr = splitKeyword(expr);

    if (top)
      expectError(expr, atTop, top, 'top-level context, ' + desc);
    else
      expectSuccess(error(expr), 'top-level context, ' + desc);

    if (fun)
      expectError(expr, inFun, fun, 'function context, ' + desc);
    else
      expectSuccess(error(inFun(expr)), 'function context, ' + desc);

    if (gen)
      expectError(expr, inGen, gen, 'generator context, ' + desc);
    else
      expectSuccess(error(inGen(expr)), 'generator context, ' + desc);

    if (genexp)
      expectError(expr, inGenExp, genexp, 'genexp context, ' + desc);
    else
      expectSuccess(error(inGenExp(expr)), 'genexp context, ' + desc);
  }

  exitFunc ('test');
}
