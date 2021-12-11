/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 226078;
var summary = 'Do not Crash @ js_Interpret 3127f864';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 

function SetLangHead(l){
  with(p){
    for(var i in x)
      if(getElementById("TxtH"+i)!=undefined)
        printStatus('huh');
  }
}
x=[0,1,2,3];
p={getElementById: function (id){
  printStatus(id);
  if (typeof dis === "function") {
    dis(SetLangHead);
  }
  return undefined;
}};
SetLangHead(1);

reportCompare(expect, actual, summary);
