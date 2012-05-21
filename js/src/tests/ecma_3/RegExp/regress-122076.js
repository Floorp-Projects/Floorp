/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    12 Feb 2002
 * SUMMARY: Don't crash on invalid regexp literals /  \\/  /
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=122076
 * The function checkURL() below sometimes caused a compile-time error:
 *
 *         SyntaxError: unterminated parenthetical (:
 *
 * However, sometimes it would cause a crash instead. The presence of
 * other functions below is merely fodder to help provoke the crash.
 * The constant |STRESS| is number of times we'll try to crash on this.
 *
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 122076;
var summary = "Don't crash on invalid regexp literals /  \\/  /";
var STRESS = 10;
var sEval = '';

printBugNumber(BUGNUMBER);
printStatus(summary);


sEval += 'function checkDate()'
sEval += '{'
sEval += 'return (this.value.search(/^[012]?\d\/[0123]?\d\/[0]\d$/) != -1);'
sEval += '}'

sEval += 'function checkDNSName()'
sEval += '{'
sEval += '  return (this.value.search(/^([\w\-]+\.)+([\w\-]{2,3})$/) != -1);'
sEval += '}'

sEval += 'function checkEmail()'
sEval += '{'
sEval += '  return (this.value.search(/^([\w\-]+\.)*[\w\-]+@([\w\-]+\.)+([\w\-]{2,3})$/) != -1);'
sEval += '}'

sEval += 'function checkHostOrIP()'
sEval += '{'
sEval += '  if (this.value.search(/^([\w\-]+\.)+([\w\-]{2,3})$/) == -1)'
sEval += '    return (this.value.search(/^[1-2]?\d{1,2}\.[1-2]?\d{1,2}\.[1-2]?\d{1,2}\.[1-2]?\d{1,2}$/) != -1);'
sEval += '  else'
sEval += '    return true;'
sEval += '}'

sEval += 'function checkIPAddress()'
sEval += '{'
sEval += '  return (this.value.search(/^[1-2]?\d{1,2}\.[1-2]?\d{1,2}\.[1-2]?\d{1,2}\.[1-2]?\d{1,2}$/) != -1);'
sEval += '}'

sEval += 'function checkURL()'
sEval += '{'
sEval += '  return (this.value.search(/^(((https?)|(ftp)):\/\/([\-\w]+\.)+\w{2,4}(\/[%\-\w]+(\.\w{2,})?)*(([\w\-\.\?\\/\*\$+@&#;`~=%!]*)(\.\w{2,})?)*\/?)$/) != -1);'
sEval += '}'


for (var i=0; i<STRESS; i++)
{
  try
  {
    eval(sEval);
  }
  catch(e)
  {
  }
}

reportCompare('No Crash', 'No Crash', '');
