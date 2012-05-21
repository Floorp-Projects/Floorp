// |reftest| skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350531;
var summary = 'exhaustively test parenthesization of binary operator subsets';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
// Translated from permcomb.py, found at
// http://biotech.embl-ebi.ac.uk:8400/sw/common/share/python/examples/dstruct/classics/permcomb.py
// by searching for "permcomb.py".
//
// This shows bugs, gaps, and verbosities in JS compared to Python:
// 1. Lack of range([start, ] end[, step]).
// 2. ![] => false, indeed !<any-object> => false.
// 3. Missing append or push for strings (if append, then we'd want append for
//    arrays too).
// 4. Missing slice operator syntax s[i:j].
// 5. Lack of + for array concatenation.

  String.prototype.push = function (str) { return this + str; };

  function permute(list) {
    if (!list.length)                                   // shuffle any sequence
      return [list];                                    // empty sequence
    var res = [];
    for (var i = 0, n = list.length; i < n; i++) {      // delete current node
      var rest = list.slice(0, i).concat(list.slice(i+1));
      for each (var x in permute(rest))                 // permute the others
        res.push(list.slice(i, i+1).concat(x));         // add node at front
    }
    return res;
  }

  function subset(list, size) {
    if (size == 0 || !list.length)                      // order matters here
      return [list.slice(0, 0)];                        // an empty sequence
    var result = [];
    for (var i = 0, n = list.length; i < n; i++) {
      var pick = list.slice(i, i+1);                    // sequence slice
      var rest = list.slice(0, i).concat(list.slice(i+1)); // keep [:i] part
      for each (var x in subset(rest, size-1))
	result.push(pick.concat(x));
    }
    return result;
  }

  function combo(list, size) {
    if (size == 0 || !list.length)                      // order doesn't matter
      return [list.slice(0, 0)];                        // xyz == yzx
    var result = [];
    for (var i = 0, n = (list.length - size) + 1; i < n; i++) {
      // iff enough left
      var pick = list.slice(i, i+1);
      var rest = list.slice(i+1);                       // drop [:i] part
      for each (var x in combo(rest, size - 1))
	result.push(pick.concat(x));
    }
    return result;
  }


// Generate all subsets of distinct binary operators and join them from left
// to right, parenthesizing minimally.  Decompile, recompile, compress spaces
// and compare to test correct parenthesization.

//  load("permcomb.js");

  var bops = [
    ["=", "|=", "^=", "&=", "<<=", ">>=", ">>>=", "+=", "-=", "*=", "/=", "%="],
    ["||"],
    ["&&"],
    ["|"],
    ["^"],
    ["&"],
    ["==", "!=", "===", "!=="],
    ["<", "<=", ">=", ">", "in", "instanceof"],
    ["<<", ">>", ">>>"],
    ["+", "-"],
    ["*", "/", "%"],
    ];

  var prec = {};
  var aops = [];

  for (var i = 0; i < bops.length; i++) {
    for (var j = 0; j < bops[i].length; j++) {
      var k = bops[i][j];
      prec[k] = i;
      aops.push(k);
    }
  }

// Theoretically all subsets of size 2 should be enough to test, but in case
// there's some large-scale bug, try up to 5 (or higher? The cost in memory is
// factorially explosive).
next_subset:
  for (i = 2; i < 5; i++) {
    var sets = subset(aops, i);
    gc();

    for each (var set in sets) {
      //print('for each set in sets: ' + (uneval(set)) );
      var src = "(function () {";
      for (j in set) {
        var op = set[j], op2 = set[j-1];

        // Precedence 0 is for assignment ops, which are right-
        // associative, so don't force left associativity using
        // parentheses.
        if (prec[op] && prec[op] < prec[op2])
          src += "(";
      }
      src += "x ";
      for (j in set) {
        var op = set[j], op2 = set[j+1];

        // Parenthesize only if not right-associative (precedence 0) and
        // the next op is higher precedence than current.
        var term = (prec[op] && prec[op] < prec[op2]) ? " x)" : " x";

        src += op + term;
        if (j < set.length - 1)
          src += " ";
      }
      src += ";})";
      try {
        var ref = uneval(eval(src)).replace(/\s+/g, ' ');
        if (ref != src) {
          actual += "BROKEN! input: " + src + " output: " + ref + " ";
          print("BROKEN! input: " + src + " output: " + ref);
          break next_subset;
        }
      } catch (e) {}
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
