/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = '53268';
var status = 'Testing scope after changing obj.__proto__';
var expect= '';
var actual = '';
var obj = {};
const five = 5;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ("test");
  printBugNumber(BUGNUMBER);
  printStatus (status);


  status= 'Step 1:  setting obj.__proto__ = global object';
  obj.__proto__ = this;

  actual = obj.five;
  expect=5;
  reportCompare (expect, actual, status);

  obj.five=1;
  actual = obj.five;
  expect=5;
  reportCompare (expect, actual, status);



  status= 'Step 2:  setting obj.__proto__ = null';
  obj.__proto__ = null;

  actual = obj.five;
  expect=undefined;
  reportCompare (expect, actual, status);

  obj.five=2;
  actual = obj.five;
  expect=2;
  reportCompare (expect, actual, status);


 
  status= 'Step 3:  setting obj.__proto__  to global object again';
  obj.__proto__ = this;

  actual = obj.five;
  expect=2;  //<--- (FROM STEP 2 ABOVE)
  reportCompare (expect, actual, status);

  obj.five=3;
  actual = obj.five;
  expect=3;
  reportCompare (expect, actual, status);



  status= 'Step 4:  setting obj.__proto__   to  null again';
  obj.__proto__ = null;

  actual = obj.five;
  expect=3;  //<--- (FROM STEP 3 ABOVE)
  reportCompare (expect, actual, status);

  obj.five=4;
  actual = obj.five;
  expect=4;
  reportCompare (expect, actual, status);


  exitFunc ("test");
}
