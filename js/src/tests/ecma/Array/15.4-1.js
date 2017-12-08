/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4-1.js
   ECMA Section:       15.4 Array Objects

   Description:        Every Array object has a length property whose value
   is always an integer with positive sign and less than
   Math.pow(2,32).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.4-1";
var TITLE   = "Array Objects";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase("var myarr = new Array(); myarr[Math.pow(2,32)-2]='hi'; myarr[Math.pow(2,32)-2]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,32)-2]='hi'; myarr[Math.pow(2,32)-2]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,32)-2]='hi'; myarr.length",
             (Math.pow(2,32)-1),
             eval("var myarr = new Array(); myarr[Math.pow(2,32)-2]='hi'; myarr.length")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,32)-3]='hi'; myarr[Math.pow(2,32)-3]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,32)-3]='hi'; myarr[Math.pow(2,32)-3]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,32)-3]='hi'; myarr.length",
             (Math.pow(2,32)-2),
             eval("var myarr = new Array(); myarr[Math.pow(2,32)-3]='hi'; myarr.length")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)-2]='hi'; myarr[Math.pow(2,31)-2]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,31)-2]='hi'; myarr[Math.pow(2,31)-2]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)-2]='hi'; myarr.length",
             (Math.pow(2,31)-1),
             eval("var myarr = new Array(); myarr[Math.pow(2,31)-2]='hi'; myarr.length")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)-1]='hi'; myarr[Math.pow(2,31)-1]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,31)-1]='hi'; myarr[Math.pow(2,31)-1]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)-1]='hi'; myarr.length",
             (Math.pow(2,31)),
             eval("var myarr = new Array(); myarr[Math.pow(2,31)-1]='hi'; myarr.length")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)]='hi'; myarr[Math.pow(2,31)]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,31)]='hi'; myarr[Math.pow(2,31)]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,31)]='hi'; myarr.length",
             (Math.pow(2,31)+1),
             eval("var myarr = new Array(); myarr[Math.pow(2,31)]='hi'; myarr.length")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,30)-2]='hi'; myarr[Math.pow(2,30)-2]",
             "hi",
             eval("var myarr = new Array(); myarr[Math.pow(2,30)-2]='hi'; myarr[Math.pow(2,30)-2]")
  );

new TestCase("var myarr = new Array(); myarr[Math.pow(2,30)-2]='hi'; myarr.length",
             (Math.pow(2,30)-1),
             eval("var myarr = new Array(); myarr[Math.pow(2,30)-2]='hi'; myarr.length")
  );

test();

