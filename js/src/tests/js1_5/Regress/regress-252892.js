/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 252892;
var summary = 'for (var i in o) in heavyweight function f should define i in f\'s activation';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var status; 

var dodis;

function f1(o){for(var x in o)printStatus(o[x]); return x}
function f2(o){with(this)for(var x in o)printStatus(o[x]); return x}
function f2novar(o){with(this)for(x in o)printStatus(o[x]); return x}
function f3(i,o){for(var x=i in o)printStatus(o[x]); return x}
function f4(i,o){with(this)for(var x=i in o)printStatus(o[x]); return x}

const f1src =
  "function f1(o) {\n" +
  "    for (var x in o) {\n" +
  "        printStatus(o[x]);\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f2src =
  "function f2(o) {\n" +
  "    with (this) {\n" +
  "        for (var x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f2novarsrc =
  "function f2novar(o) {\n" +
  "    with (this) {\n" +
  "        for (x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f3src =
  "function f3(i, o) {\n" +
  "    var x = i;\n" +
  "    for (x in o) {\n" +
  "        printStatus(o[x]);\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

const f4src =
  "function f4(i, o) {\n" +
  "    with (this) {\n" +
  "        var x = i;\n" +
  "        for (x in o) {\n" +
  "            printStatus(o[x]);\n" +
  "        }\n" +
  "    }\n" +
  "    return x;\n" +
  "}";

var t=0;
function assert(c)
{
  ++t;

  status = summary + ' ' + inSection(t);
  expect = true;
  actual = c;

  if (!c)
  {
    printStatus(t + " FAILED!");
  }
  reportCompare(expect, actual, summary);
}

if (dodis && this.dis) dis(f1);
assert(f1 == f1src);

if (dodis && this.dis) dis(f2);
assert(f2 == f2src);

if (dodis && this.dis) dis(f2novar);
assert(f2novar == f2novarsrc);

if (dodis && this.dis) dis(f3);
assert(f3 == f3src);

if (dodis && this.dis) dis(f4);
assert(f4 == f4src);

assert(f1([]) == undefined);

assert(f1(['first']) == 0);

assert(f2([]) == undefined);

assert(f2(['first']) == 0);

assert(f3(42, []) == 42);

assert(f3(42, ['first']) == 0);

assert(f4(42, []) == 42);

assert(f4(42, ['first']) == 0);

this.x = 41;

assert(f2([]) == undefined);

assert(f2novar([]) == 41);

assert(f2(['first']) == undefined);

assert(f2novar(['first']) == 0);
