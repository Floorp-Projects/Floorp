/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 481436;
var summary = 'TM: Do not crash @ FlushNativeStackFrame/JS_XDRNewMem';
var actual = '';
var expect = '';


printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

function search(m, n) {
  if (m.name == n)
    return m;
  for (var i = 0; i < m.items.length; i++)
    if (m.items[i].type == 'M')
      search(m.items[i], n);
}

function crash() {
  for (var i = 0; i < 2; i++) {
    var root = {name: 'root', type: 'M', items: [{}]};
    search(root, 'x');
    root.items.push({name: 'tim', type: 'M', items: []});
    search(root, 'x');
    search(root, 'x');
  }
}

crash();

jit(false);

reportCompare(expect, actual, summary);
