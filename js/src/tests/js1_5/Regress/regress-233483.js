/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 233483;
var summary = 'Don\'t crash with null properties - Browser only';
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document != 'undefined')
{ 
  // delay test driver end
  gDelayTestDriverEnd = true;
  window.onload = onLoad;
}
else
{
  actual = 'No Crash';
  reportCompare(expect, actual, summary);
}

function onLoad() {
  setform();
  var a=new Array();
  var forms = document.getElementsByTagName('form');
  a[a.length]=forms[0];
  var s=a.toString();

  actual = 'No Crash';

  reportCompare(expect, actual, summary);
  gDelayTestDriverEnd = false;
  jsTestDriverEnd();

}

function setform()
{
  var form = document.body.appendChild(document.createElement('form'));
  var input = form.appendChild(document.createElement('input'));
  input.setAttribute('id', 'test');
  input.setAttribute('value', '1232');

  var result = form.toString();

}
