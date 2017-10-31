// |reftest| skip -- Yarr doesn't bail on complex regexps.
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 330569;
var summary = 'RegExp - throw InternalError on too complex regular expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var s;
  expect = 'InternalError: regular expression too complex';
 
  s = '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">' +
    '<html>\n' +
    '<head>\n' +
    '<meta http-equiv="content-type" content="text/html; charset=windows-1250">\n' +
    '<meta name="generator" content="PSPad editor, www.pspad.com">\n' +
    '<title></title>\n'+
    '</head>\n' +
    '<body>\n' +
    '<!-- hello -->\n' +
    '<script language="JavaScript">\n' +
    'var s = document. body. innerHTML;\n' +
    'var d = s. replace (/<!--(.*|\n)*-->/, "");\n' +
    'alert (d);\n' +
    '</script>\n' +
    '</body>\n' +
    '</html>\n';

  try
  {
    /<!--(.*|\n)*-->/.exec(s);
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': /<!--(.*|\\n)*-->/.exec(s)');

  function testre( re, n ) {
    for ( var i= 0; i <= n; ++i ) {
      re.test( Array( i+1 ).join() );
    }
  }

  try
  {
    testre( /(?:,*)*x/, 22 );
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': testre( /(?:,*)*x/, 22 )');

  try
  {
    testre( /(?:,|,)*x/, 22 );
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': testre( /(?:,|,)*x/, 22 )');

  try
  {
    testre( /(?:,|,|,|,|,)*x/, 10 );
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': testre( /(?:,|,|,|,|,)*x/, 10 )');
}
