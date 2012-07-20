// |reftest| pref(javascript.options.xml.content,true) skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 319872;
var summary = 'Do not Crash in jsxml.c';
var actual = 'No Crash';
var expect = /(No Crash|InternalError: allocation size overflow)/;

printBugNumber(BUGNUMBER);
START(summary);
printStatus ("Expect either no error or out of memory");
expectExitCode(0);
expectExitCode(5);

try
{
  var i,m,str;
  str="<a xmlns:v=\"";
  m="";

  for (i=0;i<(1024*1024)/2;i++)
    m += "\n";

  for(i=0;i<500;i++)
    str += m ;

  str += "\">f00k</a>";

  var xx = new XML(str);

  printStatus(xx.toXMLString());
}
catch(ex)
{
  actual = ex + '';
  print(actual);
}
reportMatch(expect, actual, summary);
