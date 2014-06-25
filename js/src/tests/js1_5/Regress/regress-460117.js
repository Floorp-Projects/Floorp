/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 460117;
var summary = 'TM: hasOwnProperty with JIT';
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

  function t(o, proplist) {
    var props=proplist.split(/\s+/g);
    for (var i=0, len=props.length; i<len; i++) {
      if (o.hasOwnProperty(props[i])) {
        // do something
      } else {
        actual += (props[i]+': '+o.hasOwnProperty(props[i]));
      }
    }
  };

  t({ bar: 123, baz: 123, quux: 123 }, 'bar baz quux');

  reportCompare(expect, actual, summary + ' : nonjit');

  jit(true);

  t({ bar: 123, baz: 123, quux: 123 }, 'bar baz quux');

  jit(false);

  reportCompare(expect, actual, summary + ' : jit');

  exitFunc ('test');
}
