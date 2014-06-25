/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 634472;
var summary = 'contextual restrictions for yield and arguments';
var actual = '';
var expect = '';


function error(str) {
  let base;
  try {
    // the following line must not be broken up into multiple lines
    base = (function(){try{eval('throw new Error()')}catch(e){return e.lineNumber}})(); eval(str);
    return null;
  } catch (e) {
    e.lineNumber = e.lineNumber - base + 1;
    return e;
  }
}

const JSMSG_GENEXP_YIELD         = error("(function(){((yield) for (x in []))})").message;
const JSMSG_TOP_YIELD            = error("yield").message;
const JSMSG_YIELD_PAREN          = error("(function(){yield, 1})").message;
const JSMSG_YIELD_FOR            = error("(function(){yield for})").message;
const JSMSG_BAD_GENERATOR_SYNTAX = error("(1, x for (x in []))").message;

const cases = [
  // yield expressions
  { expr: "yield",        top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "simple yield" },
  { expr: "yield 1",      top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg" },
  { expr: "1, yield",     top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "simple yield at end of list" },
  { expr: "1, yield 2",   top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg at end of list" },
  { expr: "yield, 1",     top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN, gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list" },
  { expr: "yield 1, 2",   top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN, gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list" },
  { expr: "(yield)",      top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "simple yield, parenthesized" },
  { expr: "(yield 1)",    top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg, parenthesized" },
  { expr: "(1, yield)",   top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "simple yield at end of list, parenthesized" },
  { expr: "(1, yield 2)", top: JSMSG_TOP_YIELD, fun: null,              gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg at end of list, parenthesized" },
  { expr: "(yield, 1)",   top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN, gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list, parenthesized" },
  { expr: "(yield 1, 2)", top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN, gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list, parenthesized" },

  // deeply nested yield expressions
  { expr: "((((yield))))",   top: JSMSG_TOP_YIELD, fun: null, gen: JSMSG_GENEXP_YIELD, desc: "deeply nested yield" },
  { expr: "((((yield 1))))", top: JSMSG_TOP_YIELD, fun: null, gen: JSMSG_GENEXP_YIELD, desc: "deeply nested yield w/ arg" },

  // arguments
  { expr: "arguments",    top: null, fun: null, gen: null, desc: "arguments in list" },
  { expr: "1, arguments", top: null, fun: null, gen: null, desc: "arguments in list" },

  // yield in generator expressions
  { expr: "(yield for (x in []))",           top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_FOR,    gen: JSMSG_YIELD_FOR,      desc: "simple yield in genexp" },
  { expr: "(yield 1 for (x in []))",         top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg in genexp" },
  { expr: "(yield, 1 for (x in []))",        top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list in genexp" },
  { expr: "(yield 1, 2 for (x in []))",      top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list in genexp" },
  { expr: "(1, yield for (x in []))",        top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_FOR,    gen: JSMSG_YIELD_FOR,      desc: "simple yield at end of list in genexp" },
  { expr: "(1, yield 2 for (x in []))",      top: JSMSG_TOP_YIELD, fun: { simple: JSMSG_GENEXP_YIELD, call: JSMSG_GENEXP_YIELD },
                                                                                            gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg at end of list in genexp" },
  { expr: "((yield) for (x in []))",         top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "simple yield, parenthesized in genexp" },
  { expr: "((yield 1) for (x in []))",       top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg, parenthesized in genexp" },
  { expr: "(1, (yield) for (x in []))",      top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "simple yield, parenthesized in list in genexp" },
  { expr: "(1, (yield 2) for (x in []))",    top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg, parenthesized in list in genexp" },
  { expr: "((1, yield) for (x in []))",      top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "simple yield at end of list, parenthesized in genexp" },
  { expr: "((1, yield 2) for (x in []))",    top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg at end of list, parenthesized in genexp" },
  { expr: "(1, (2, yield) for (x in []))",   top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "simple yield at end of list, parenthesized in list in genexp" },
  { expr: "(1, (2, yield 3) for (x in []))", top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg at end of list, parenthesized in list in genexp" },
  { expr: "((yield, 1) for (x in []))",      top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list, parenthesized in genexp" },
  { expr: "((yield 1, 2) for (x in []))",    top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list, parenthesized in genexp" },
  { expr: "(1, (yield, 2) for (x in []))",   top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list, parenthesized in list in genexp" },
  { expr: "(1, (yield 2, 3) for (x in []))", top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list, parenthesized in list in genexp" },

  // deeply nested yield in generator expressions
  { expr: "((((1, yield 2))) for (x in []))",               top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "deeply nested yield in genexp" },
  { expr: "((((1, yield 2)) for (x in [])) for (y in []))", top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "deeply nested yield in multiple genexps" },

  // arguments in generator expressions
  { expr: "(arguments for (x in []))",         top: null,                       fun: null,                       gen: null,                       desc: "simple arguments in genexp" },
  { expr: "(1, arguments for (x in []))",      top: JSMSG_BAD_GENERATOR_SYNTAX, fun: JSMSG_BAD_GENERATOR_SYNTAX, gen: JSMSG_BAD_GENERATOR_SYNTAX, desc: "arguments in list in genexp" },
  { expr: "((arguments) for (x in []))",       top: null,                       fun: null,                       gen: null,                       desc: "arguments, parenthesized in genexp" },
  { expr: "(1, (arguments) for (x in []))",    top: JSMSG_BAD_GENERATOR_SYNTAX, fun: JSMSG_BAD_GENERATOR_SYNTAX, gen: JSMSG_BAD_GENERATOR_SYNTAX, desc: "arguments, parenthesized in list in genexp" },
  { expr: "((1, arguments) for (x in []))",    top: null,                       fun: null,                       gen: null,                       desc: "arguments in list, parenthesized in genexp" },
  { expr: "(1, (2, arguments) for (x in []))", top: JSMSG_BAD_GENERATOR_SYNTAX, fun: JSMSG_BAD_GENERATOR_SYNTAX, gen: JSMSG_BAD_GENERATOR_SYNTAX, desc: "arguments in list, parenthesized in list in genexp" },

  // deeply nested arguments in generator expressions
  { expr: "((((1, arguments))) for (x in []))",               top: null, fun: null, gen: null, desc: "deeply nested arguments in genexp" },
  { expr: "((((1, arguments)) for (x in [])) for (y in []))", top: null, fun: null, gen: null, desc: "deeply nested arguments in multiple genexps" },

  // legal yield/arguments in nested function
  { expr: "((function() { yield }) for (x in []))",           top: null, fun: null, gen: null, desc: "legal yield in nested function" },
  { expr: "((function() { arguments }) for (x in []))",       top: null, fun: null, gen: null, desc: "legal arguments in nested function" },
  { expr: "((function() arguments) for (x in []))",           top: null, fun: null, gen: null, desc: "legal arguments in nested expression-closure" }
];

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function splitKeyword(str) {
  return str.
         replace(/yield for/, '\nyield for\n').
         replace(/yield ([0-9])/, '\nyield $1\n').
         replace(/yield([^ ]|$)/, '\nyield\n$1').
         replace(/arguments/, '\narguments\n');
}

function expectError1(err, ctx, msg) {
  reportCompare('object', typeof err,     'exn for: ' + msg);
  reportCompare(ctx,      err.message,    'exn message for: ' + msg);
  if (ctx !== JSMSG_BAD_GENERATOR_SYNTAX)
      reportCompare(2,    err.lineNumber, 'exn token for: ' + msg);
}

function expectError(expr, call, wrapCtx, expect, msg) {
  let exps = (typeof expect === "string")
           ? { simple: expect, call: expect }
           : expect;
  expectError1(error(wrapCtx(expr)), exps.simple, msg);
  if (call)
    expectError1(error(wrapCtx(call)), exps.call, 'call argument in ' + msg);
}

function expectSuccess(err, msg) {
  reportCompare(null, err, 'parse: ' + msg);
}

function atTop(str) { return str }
function inFun(str) { return '(function(){' + str + '})' }
function inGen(str) { return '(y for (y in ' + str + '))' }

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (let i = 0, len = cases.length; i < len; i++) {
    let {expr, top, fun, gen, desc} = cases[i];

    let call = (expr[0] === "(") ? ("print" + expr) : null;

    expr = splitKeyword(expr);
    if (call)
      call = splitKeyword(call);

    if (top)
      expectError(expr, call, atTop, top, 'top-level context, ' + desc);
    else
      expectSuccess(error(expr), 'top-level context, ' + desc);

    if (fun)
      expectError(expr, call, inFun, fun, 'function context, ' + desc);
    else
      expectSuccess(error(inFun(expr)), 'function context, ' + desc);

    if (gen)
      expectError(expr, call, inGen, gen, 'genexp context, ' + desc);
    else
      expectSuccess(error(inGen(expr)), 'genexp context, ' + desc);
  }

  exitFunc ('test');
}
