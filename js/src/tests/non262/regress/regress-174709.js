/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 174709;
var summary = 'Don\'t Crash';
var actual = 'FAIL';
var expect = 'PASS';

printBugNumber(BUGNUMBER);
printStatus (summary);

/* code removed until replacement can be created. */

/* crash and burn */

var G2z=["ev","sta","fro","cha","le",[""]];
var qS0=[];
qS0.push("G"+2);
qS0.push("z");
var kJ6=this[qS0.join("")];
kJ6[0]+="a";
kJ6[1]+="rtX";
kJ6[2]+="mCha";
kJ6[3]+="rCo";
kJ6[4]+="ngt";
function heW(){}
heW.prototype={};
var b2V=new Array();
b2V.push("k");
b2V.push("J");
b2V.push(6);
var Co4=this[b2V.join("")];
Co4[0]+="l";
Co4[1]+="opu";
Co4[2]+="rCo";
Co4[3]+="deA";
Co4[4]+="h";

var Ke1=[];
Ke1.push("C");
Ke1.push("o");
Ke1.push(4);
var Qp3=this[Ke1.join("")];
Qp3[1]+="s";
Qp3[2]+="de";
Qp3[3]+="t";

var kh1=Qp3[5][Qp3[4]];

var l8q=[];
l8q.push("g".toUpperCase());
l8q.push(2);
l8q.push("z");
/* var e2k=window[l8q.join("")];*/
var e2k=eval(l8q.join(""));
for (Qp3[5][kh1] in this)
  for (Qp3[5][kh1+1] in this)
    if (Qp3[5][kh1] > Qp3[5][kh1+1] &&
        Qp3[5][kh1][e2k[4]] == Qp3[5][kh1+1][e2k[4]] &&
        Qp3[5][kh1].substr(kh1,kh1+1) == Qp3[5][kh1+1].substr(kh1,kh1+1))
      Qp3[5][kh1 + 2] = Qp3[5][kh1];

               
actual = 'PASS';

reportCompare(expect, actual, summary);
