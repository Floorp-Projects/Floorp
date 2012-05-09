/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dave Herman
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
const JSMSG_GENEXP_ARGUMENTS     = error("(function(){(arguments for (x in []))})").message;
const JSMSG_TOP_YIELD            = error("yield").message;
const JSMSG_YIELD_PAREN          = error("(function(){yield, 1})").message;
const JSMSG_GENERIC              = error("(for)").message;
const JSMSG_GENEXP_PAREN         = error("print(1, x for (x in []))").message;
const JSMSG_BAD_GENERATOR_SYNTAX = error("(1, arguments for (x in []))").message;

const JSMSG_GENEXP_MIX = { simple: JSMSG_BAD_GENERATOR_SYNTAX, call: JSMSG_GENEXP_ARGUMENTS };

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
  { expr: "arguments",         top: null, fun: null, gen: JSMSG_GENEXP_ARGUMENTS, desc: "simple arguments" },
  { expr: "1, arguments",      top: null, fun: null, gen: JSMSG_GENEXP_ARGUMENTS, desc: "arguments in list" },
  { expr: "(arguments)",       top: null, fun: null, gen: JSMSG_GENEXP_ARGUMENTS, desc: "simple arguments, parenthesized" },
  { expr: "(1, arguments)",    top: null, fun: null, gen: JSMSG_GENEXP_ARGUMENTS, desc: "arguments in list, parenthesized" },
  { expr: "((((arguments))))", top: null, fun: null, gen: JSMSG_GENEXP_ARGUMENTS, desc: "deeply nested arguments" },

  // yield in generator expressions
  { expr: "(yield for (x in []))",           top: JSMSG_TOP_YIELD, fun: JSMSG_GENERIC,      gen: JSMSG_GENERIC,      desc: "simple yield in genexp" },
  { expr: "(yield 1 for (x in []))",         top: JSMSG_TOP_YIELD, fun: JSMSG_GENEXP_YIELD, gen: JSMSG_GENEXP_YIELD, desc: "yield w/ arg in genexp" },
  { expr: "(yield, 1 for (x in []))",        top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "simple yield in list in genexp" },
  { expr: "(yield 1, 2 for (x in []))",      top: JSMSG_TOP_YIELD, fun: JSMSG_YIELD_PAREN,  gen: JSMSG_YIELD_PAREN,  desc: "yield w/ arg in list in genexp" },
  { expr: "(1, yield for (x in []))",        top: JSMSG_TOP_YIELD, fun: JSMSG_GENERIC,      gen: JSMSG_GENERIC,      desc: "simple yield at end of list in genexp" },
  { expr: "(1, yield 2 for (x in []))",      top: JSMSG_TOP_YIELD, fun: { simple: JSMSG_GENEXP_YIELD, call: JSMSG_GENEXP_PAREN },
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
  { expr: "(arguments for (x in []))",         top: JSMSG_GENEXP_ARGUMENTS, fun: JSMSG_GENEXP_ARGUMENTS, gen: JSMSG_GENEXP_ARGUMENTS, desc: "simple arguments in genexp" },
  { expr: "(1, arguments for (x in []))",      top: JSMSG_GENEXP_MIX,       fun: JSMSG_GENEXP_MIX,       gen: JSMSG_GENEXP_MIX,       desc: "arguments in list in genexp" },
  { expr: "((arguments) for (x in []))",       top: JSMSG_GENEXP_ARGUMENTS, fun: JSMSG_GENEXP_ARGUMENTS, gen: JSMSG_GENEXP_ARGUMENTS, desc: "arguments, parenthesized in genexp" },
  { expr: "(1, (arguments) for (x in []))",    top: JSMSG_GENEXP_MIX,       fun: JSMSG_GENEXP_MIX,       gen: JSMSG_GENEXP_MIX,       desc: "arguments, parenthesized in list in genexp" },
  { expr: "((1, arguments) for (x in []))",    top: JSMSG_GENEXP_ARGUMENTS, fun: JSMSG_GENEXP_ARGUMENTS, gen: JSMSG_GENEXP_ARGUMENTS, desc: "arguments in list, parenthesized in genexp" },
  { expr: "(1, (2, arguments) for (x in []))", top: JSMSG_GENEXP_MIX,       fun: JSMSG_GENEXP_MIX,       gen: JSMSG_GENEXP_MIX,       desc: "arguments in list, parenthesized in list in genexp" },

  // deeply nested arguments in generator expressions
  { expr: "((((1, arguments))) for (x in []))",               top: JSMSG_GENEXP_ARGUMENTS, fun: JSMSG_GENEXP_ARGUMENTS, gen: JSMSG_GENEXP_ARGUMENTS, desc: "deeply nested arguments in genexp" },
  { expr: "((((1, arguments)) for (x in [])) for (y in []))", top: JSMSG_GENEXP_ARGUMENTS, fun: JSMSG_GENEXP_ARGUMENTS, gen: JSMSG_GENEXP_ARGUMENTS, desc: "deeply nested arguments in multiple genexps" },

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
