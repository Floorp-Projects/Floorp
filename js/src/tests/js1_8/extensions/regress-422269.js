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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov
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
var BUGNUMBER = 422269;
var summary = 'Compile-time let block should not capture runtime references';
var actual = 'referenced only by stack and closure';
var expect = 'referenced only by stack and closure';


//-----------------------------------------------------------------------------
test();

//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function f()
  {
    let m = {sin: Math.sin};
    (function holder() { m.sin(1); })();
    return m;
  }

  if (typeof findReferences == 'undefined')
  {
    expect = actual = 'Test skipped';
    print('Test skipped. Requires findReferences function.');
  }
  else
  {
    var x = f();
    var refs = findReferences(x);

    // At this point, x should only be referenced from the stack --- the
    // variable 'x' itself, and any random things the conservative scanner
    // finds --- and possibly from the 'holder' closure, which could itself
    // be held alive for random reasons. Remove those from the refs list, and 
    // then complain if anything is left.
    for (var edge in refs) {
      // Remove references from roots, like the stack.
      if (refs[edge].every(function (r) r === null))
        delete refs[edge];
      // Remove references from the closure, which could be held alive for
      // random reasons.
      else if (refs[edge].length === 1 &&
               typeof refs[edge][0] === "function" &&
               refs[edge][0].name === "holder")
        delete refs[edge];
    }

    if (Object.keys(refs).length != 0)
        actual = "unexpected references to the result of f: " + Object.keys(refs).join(", ");
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
