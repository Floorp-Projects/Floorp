/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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

// ------- Comment #135 From Gary Kwong [:nth10sd]

// -j is not required:
// ===

  for (let i = 0; i < 9; ++i) {
    let m = i;
    if (i % 3 == 1) {
      print('' + (function() { return m; })());
    }
  }

// Assertion failure: fp2->fun && fp2->script, at ../jsinterp.cpp:5633
// Opt crash [@ js_Interpret]
// ===

  try
  {
    (x for each (c in []))
      x
      }
  catch(ex)
  {
  }

// Assertion failure: dn_kind == JSDefinition::VAR || dn_kind == JSDefinition::CONST, at ../jsemit.cpp:2187

// ===
  "" + (function(){L:if (*::*){ var x } function x(){}})

// Assertion failure: slot < StackDepth(jp->script), at ../jsopcode.cpp:1329
// ===
    "" + (function(){if (*::*){ var x } function x(){}})

// Assertion failure: (uintN)i < ss->top, at ../jsopcode.cpp:2825
// ===
    "" + (function(){{<y/>; throw <z/>;for(var x = [] in false) return } function x(){}})

// Assertion failure: ss->printer->pcstack, at ../jsopcode.cpp:909
// ===
    try
    {
      (function(){for(; (this); ((window for (x in [])) for (y in []))) 0});
    }
    catch(ex)
    {
    }
// Assertion failure: level >= tc->staticLevel, at ../jsparse.cpp:5773
// ===
  eval(uneval( function(){
        ((function()y)() for each (x in this))
          } ))

// Debug & opt crash [@ BindNameToSlot]

// -j is required:
// ===
    for (let a=0;a<3;++a) for (let b=0;b<3;++b) if ((g=a|(a%b))) with({}){}

// Assertion failure: OBJ_IS_CLONED_BLOCK(obj), at ../jsobj.cpp:2392

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
