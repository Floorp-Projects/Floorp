/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
const MISSING_PAREN        = error("(yield 1)").message;
const PAREN_PAREN          = error("(foo").message;
const FOR_OF_PAREN         = error("(for (x of y, z) w)").message;

const cases = [
// Expressions involving yield without a value, not currently implemented.  Many
// of these errors would need to be updated.
//{ expr: "yield",        top: TOP_YIELD, fun: null,              gen: GENEXP_YIELD, desc: "simple yield" },
//{ expr: "1, yield",     top: TOP_YIELD, fun: null,              gen: GENEXP_YIELD, desc: "simple yield at end of list" },
//{ expr: "yield, 1",     top: TOP_YIELD, fun: YIELD_PAREN, gen: YIELD_PAREN,  desc: "simple yield in list" },
//{ expr: "(yield)",      top: TOP_YIELD, fun: null,              gen: GENEXP_YIELD, desc: "simple yield, parenthesized" },
//{ expr: "(1, yield)",   top: TOP_YIELD, fun: null,              gen: GENEXP_YIELD, desc: "simple yield at end of list, parenthesized" },
//{ expr: "(yield, 1)",   top: TOP_YIELD, fun: YIELD_PAREN, gen: YIELD_PAREN,  desc: "simple yield in list, parenthesized" },
//{ expr: "((((yield))))",   top: TOP_YIELD, fun: null, gen: GENEXP_YIELD, desc: "deeply nested yield" },
//{ expr: "(for (x of []) yield)",           top: TOP_YIELD, fun: GENERIC,      gen: GENERIC,      desc: "simple yield in genexp" },
//{ expr: "(for (x of []) yield, 1)",        top: TOP_YIELD, fun: YIELD_PAREN,  gen: YIELD_PAREN,  desc: "simple yield in list in genexp" },
//{ expr: "(for (x of []) 1, yield)",        top: TOP_YIELD, fun: GENERIC,      gen: GENERIC,      desc: "simple yield at end of list in genexp" },
//{ expr: "(for (x of []) (yield))",         top: TOP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, desc: "simple yield, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (yield))",      top: TOP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, desc: "simple yield, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (1, yield))",      top: TOP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, desc: "simple yield at end of list, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (2, yield))",   top: TOP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, desc: "simple yield at end of list, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (yield, 1))",      top: TOP_YIELD, fun: YIELD_PAREN,  gen: YIELD_PAREN,  desc: "simple yield in list, parenthesized in genexp" },
//{ expr: "(for (x of []) 1, (yield, 2))",   top: TOP_YIELD, fun: YIELD_PAREN,  gen: YIELD_PAREN,  desc: "simple yield in list, parenthesized in list in genexp" },
//{ expr: "(for (x of []) (function*() { yield }))",           top: null, fun: null, gen: null, desc: "legal yield in nested function" },

  // yield expressions
  { expr: "yield 1",      top: MISSING_SEMI, fun: MISSING_SEMI, gen: null, genexp: GENEXP_YIELD, desc: "yield w/ arg" },
  { expr: "1, yield 2",   top: MISSING_SEMI, fun: MISSING_SEMI, gen: null, genexp: FOR_OF_PAREN, desc: "yield w/ arg at end of list" },
  { expr: "yield 1, 2",   top: MISSING_SEMI, fun: MISSING_SEMI, gen: null, genexp: FOR_OF_PAREN, desc: "yield w/ arg in list" },
  { expr: "(yield 1)",    top: MISSING_PAREN, fun: MISSING_PAREN, gen: null, genexp: GENEXP_YIELD, desc: "yield w/ arg, parenthesized" },
  { expr: "(1, yield 2)", top: MISSING_PAREN, fun: MISSING_PAREN, gen: null, genexp: GENEXP_YIELD, desc: "yield w/ arg at end of list, parenthesized" },
  { expr: "(yield 1, 2)", top: MISSING_PAREN, fun: MISSING_PAREN, gen: null, genexp: YIELD_PAREN, desc: "yield w/ arg in list, parenthesized" },

  // deeply nested yield expressions
  { expr: "((((yield 1))))", top: MISSING_PAREN, fun: MISSING_PAREN, gen: null, genexp: GENEXP_YIELD, desc: "deeply nested yield w/ arg" },

  // arguments
  { expr: "arguments",    top: null, fun: null, gen: null, genexp: null, desc: "arguments in list" },
  { expr: "1, arguments", top: null, fun: null, gen: null, genexp: FOR_OF_PAREN, desc: "arguments in list" },

  // yield in generator expressions
  { expr: "(for (x of []) yield 1)",         top: GENEXP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "yield w/ arg in genexp" },
  { expr: "(for (x of []) yield 1, 2)",      top: GENEXP_YIELD, fun: GENEXP_YIELD,  gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "yield w/ arg in list in genexp" },
  { expr: "(for (x of []) 1, yield 2)",      top: PAREN_PAREN, fun: PAREN_PAREN,  gen: PAREN_PAREN, genexp: PAREN_PAREN, desc: "yield w/ arg at end of list in genexp" },
  { expr: "(for (x of []) (yield 1))",       top: GENEXP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "yield w/ arg, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (yield 2))",    top: PAREN_PAREN, fun: PAREN_PAREN, gen: PAREN_PAREN, genexp: PAREN_PAREN, desc: "yield w/ arg, parenthesized in list in genexp" },
  { expr: "(for (x of []) (1, yield 2))",    top: GENEXP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "yield w/ arg at end of list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (2, yield 3))", top: PAREN_PAREN, fun: PAREN_PAREN, gen: PAREN_PAREN, genexp: PAREN_PAREN, desc: "yield w/ arg at end of list, parenthesized in list in genexp" },
  { expr: "(for (x of []) (yield 1, 2))",    top: YIELD_PAREN, fun: YIELD_PAREN, gen: YIELD_PAREN, genexp: YIELD_PAREN, desc: "yield w/ arg in list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (yield 2, 3))", top: PAREN_PAREN, fun: PAREN_PAREN, gen: PAREN_PAREN, genexp: PAREN_PAREN, desc: "yield w/ arg in list, parenthesized in list in genexp" },

  // deeply nested yield in generator expressions
  { expr: "(for (x of []) (((1, yield 2))))",               top: GENEXP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "deeply nested yield in genexp" },
  { expr: "(for (y of []) (for (x of []) ((1, yield 2))))", top: GENEXP_YIELD, fun: GENEXP_YIELD, gen: GENEXP_YIELD, genexp: GENEXP_YIELD, desc: "deeply nested yield in multiple genexps" },

  // arguments in generator expressions
  { expr: "(for (x of []) arguments)",         top: null, fun: null, gen: null, genexp: null, desc: "simple arguments in genexp" },
  { expr: "(for (x of []) 1, arguments)",      top: BAD_GENERATOR_SYNTAX, fun: BAD_GENERATOR_SYNTAX, gen: BAD_GENERATOR_SYNTAX, genexp: BAD_GENERATOR_SYNTAX, desc: "arguments in list in genexp" },
  { expr: "(for (x of []) (arguments))",       top: null, fun: null, gen: null, genexp: null, desc: "arguments, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (arguments))",    top: BAD_GENERATOR_SYNTAX, fun: BAD_GENERATOR_SYNTAX, gen: BAD_GENERATOR_SYNTAX, genexp: BAD_GENERATOR_SYNTAX, desc: "arguments, parenthesized in list in genexp" },
  { expr: "(for (x of []) (1, arguments))",    top: null, fun: null, gen: null, genexp: null, desc: "arguments in list, parenthesized in genexp" },
  { expr: "(for (x of []) 1, (2, arguments))", top: BAD_GENERATOR_SYNTAX, fun: BAD_GENERATOR_SYNTAX, gen: BAD_GENERATOR_SYNTAX, genexp: BAD_GENERATOR_SYNTAX, desc: "arguments in list, parenthesized in list in genexp" },

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

function expectError1(err, ctx, msg) {
  reportCompare('object', typeof err,     'exn for: ' + msg);
  reportCompare(ctx,      err.message,    'exn message for: ' + msg);
  if (ctx !== BAD_GENERATOR_SYNTAX && ctx != PAREN_PAREN && ctx != FOR_OF_PAREN)
      reportCompare(2,    err.lineNumber, 'exn token for: ' + msg);
}

function expectError(expr, wrapCtx, expect, msg) {
  expectError1(error(wrapCtx(expr)), expect, msg);
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
