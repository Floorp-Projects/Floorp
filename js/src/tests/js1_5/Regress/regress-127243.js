// |reftest| skip-if(xulRuntime.OS=="WINNT"&&isDebugBuild) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 127243;
var summary = 'Do not crash on watch';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof window != 'undefined' && typeof document != 'undefined') 
{
  // delay test driver end
  gDelayTestDriverEnd = true;
  window.addEventListener('load', handleLoad, false);
}
else
{
  printStatus('This test must be run in the browser');
  reportCompare(expect, actual, summary);

}

var div;

function handleLoad()
{
  div = document.createElement('div');
  document.body.appendChild(div);
  div.setAttribute('id', 'id1');
  div.style.width = '50px';
  div.style.height = '100px';
  div.style.overflow = 'auto';

  for (var i = 0; i < 5; i++)
  {
    var p = document.createElement('p');
    var t = document.createTextNode('blah');
    p.appendChild(t);
    div.appendChild(p);
  }

  div.watch('scrollTop', wee);

  setTimeout('setScrollTop()', 1000);
}

function wee(id, oldval, newval)
{
  var t = document.createTextNode('setting ' + id +
                                  ' value ' + div.scrollTop +
                                  ' oldval ' + oldval +
                                  ' newval ' + newval);
  var p = document.createElement('p');
  p.appendChild(t);
  document.body.appendChild(p);

  return newval;
}

function setScrollTop()
{
  div.scrollTop = 42;

  reportCompare(expect, actual, summary);

  gDelayTestDriverEnd = false;
  jsTestDriverEnd();

}
