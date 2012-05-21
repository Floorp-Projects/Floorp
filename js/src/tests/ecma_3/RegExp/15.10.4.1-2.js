/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 26 November 2000
 *
 *
 *SUMMARY: Passing a RegExp object to a RegExp() constructor.
 *This test arose from Bugzilla bug 61266. The ECMA3 section is:      
 *
 *  15.10.4.1 new RegExp(pattern, flags)
 *
 *  If pattern is an object R whose [[Class]] property is "RegExp" and
 *  flags is undefined, then let P be the pattern used to construct R
 *  and let F be the flags used to construct R. If pattern is an object R
 *  whose [[Class]] property is "RegExp" and flags is not undefined,
 *  then throw a TypeError exception. Otherwise, let P be the empty string 
 *  if pattern is undefined and ToString(pattern) otherwise, and let F be
 *  the empty string if flags is undefined and ToString(flags) otherwise.
 *
 *
 *The current test will check the first scenario outlined above:
 *
 *   "pattern" is itself a RegExp object R
 *   "flags"  is undefined
 *
 * We check that a new RegExp object obj2 defined from these parameters
 * is morally the same as the original RegExp object obj1. Of course, they
 * can't be equal as objects - so we check their enumerable properties...
 *
 * In this test, the initial RegExp object obj1 will not include a
 * flag.  This test is identical to test 15.10.4.1-1.js, except that
 * here we use this syntax:
 *
 *                  obj2 = new RegExp(obj1, undefined);
 *
 * instead of:
 *
 *                  obj2 = new RegExp(obj1);
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = '61266';
var summary = 'Passing a RegExp object to a RegExp() constructor';
var statprefix = 'Applying RegExp() twice to pattern ';
var statsuffix =  '; testing property ';
var singlequote = "'";
var i = -1; var s = '';
var obj1 = {}; var obj2 = {};
var status = ''; var actual = ''; var expect = ''; var msg = '';
var patterns = new Array(); 


// various regular expressions to try -
patterns[0] = '';
patterns[1] = 'abc';
patterns[2] = '(.*)(3-1)\s\w';
patterns[3] = '(.*)(...)\\s\\w';
patterns[4] = '[^A-Za-z0-9_]';
patterns[5] = '[^\f\n\r\t\v](123.5)([4 - 8]$)';



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (i in patterns)
  {
    s = patterns[i];
    status =getStatus(s);  
    obj1 = new RegExp(s);
    obj2 = new RegExp(obj1, undefined);  // see introduction to bug
 
    reportCompare (obj1 + '', obj2 + '', status);
  }

  exitFunc ('test');
}


function getStatus(regexp)
{
  return (statprefix  +  quote(regexp) +  statsuffix);
}


function quote(text)
{
  return (singlequote  +  text  + singlequote);
}
