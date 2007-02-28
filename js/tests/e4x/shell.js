/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Rob Ginda rginda@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var FAILED = "FAILED!: ";
var STATUS = "STATUS: ";
var BUGNUMBER = "BUGNUMBER: ";
var VERBOSE = false;
var SECT_PREFIX = 'Section ';
var SECT_SUFFIX = ' of test -';

if (typeof options != 'undefined' &&
    options().indexOf('xml') < 0)
{
    // automatically turn on e4x support
    options('xml');
}

/*
 * The test driver searches for such a phrase in the test output.
 * If such phrase exists, it will set n as the expected exit code.
 */
function expectExitCode(n)
{

    print('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');

}

/*
 * Statuses current section of a test
 */
function inSection(x)
{
    return SECT_PREFIX + x + SECT_SUFFIX;
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (section, msg)
{
    msg = inSection(section)+"\n"+msg;
    var lines = msg.split ("\n");
    for (var i=0; i<lines.length; i++)
        print (FAILED + lines[i]);
}

/*
 * Print a non-failure message.
 */
function printStatus (msg)
{
    msg = msg.toString();
    var lines = msg.split ("\n");
    var l;

    for (var i=0; i<lines.length; i++)
        print (STATUS + lines[i]);

}

/*
 * Print a bugnumber message.
 */
function printBugNumber (num)
{
  print (BUGNUMBER + num);
}

function toPrinted(value)
{
  if (typeof value == "xml") {
    return value.toXMLString();
  } else {
    return String(value);
  }
}

function START(summary)
{
  printStatus(summary);
}

function BUG(bug)
{
  printBugNumber(bug);
}

function TEST(section, expected, actual)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
    
    if (expected_t != actual_t)
        output += "Type mismatch, expected type " + expected_t + 
            ", actual type " + actual_t + "\n";
    else if (VERBOSE)
        printStatus ("Expected type '" + actual_t + "' matched actual " +
                     "type '" + expected_t + "'");

    if (expected != actual) {
	output += "Expected value:\n" + toPrinted(expected) + "\nActual value:\n" + toPrinted(actual) + "\n";
    }
    else if (VERBOSE)
        printStatus ("Expected value '" + toPrinted(actual) + "' matched actual value");

    if (output != "")
    {
        reportFailure (section, output);   
        return false;
    }
    else
    {
        print('PASSED! ' + section);
    }
    return true;
}

function TEST_XML(section, expected, actual)
{
  var actual_t = typeof actual;
  var expected_t = typeof expected;

  if (actual_t != "xml") {
    // force error on type mismatch
    return TEST(section, new XML(), actual);
  }
  
  if (expected_t == "string") {
    return TEST(section, expected, actual.toXMLString());
  }

  if (expected_t == "number") {
    return TEST(section, String(expected), actual.toXMLString());
  }

  reportFailure (section, "Bad TEST_XML usage: type of expected is "+expected_t+", should be number or string");
  return false;
}

function SHOULD_THROW(section)
{
  reportFailure(section, "Expected to generate exception, actual behavior: no exception was thrown");   
}

function END()
{
}

/* JavaScriptOptions
   encapsulate the logic for setting and retrieving the values
   of the javascript options.
   
   Note: in shell, options() takes an optional comma delimited list
   of option names, toggles the values for each option and returns the
   list of option names which were set before the call. 
   If no argument is passed to options(), it returns the current
   options with value true.

   Usage;

   // create and initialize object.
   jsOptions = new JavaScriptOptions();

   // set a particular option
   jsOptions.setOption(name, boolean);

   // reset all options to their original values.
   jsOptions.reset();
*/

function JavaScriptOptions()
{
  this.orig   = {};
  this.orig.strict = this.strict = false;
  this.orig.werror = this.werror = false;

  this.privileges = 'UniversalXPConnect UniversalPreferencesRead ' + 
                    'UniversalPreferencesWrite';

  if (typeof options == 'function')
  {
    // shell
    var optString = options();
    if (optString)
    {
      var optList = optString.split(',');
      for (var iOpt = 0; iOpt < optList.length; iOpt++)
      {
        optName = optList[iOpt];
        this[optName] = true;
      }
    }
  }
  else if (typeof netscape != 'undefined' && 'security' in netscape)
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences-service;1'];
    if (!preferences)
    {
      throw 'JavaScriptOptions: unable to get @mozilla.org/preferences-service;1';
    }

    var prefService = preferences.
      getService(Components.interfaces.nsIPrefService);

    if (!prefService)
    {
      throw 'JavaScriptOptions: unable to get nsIPrefService';
    }

    var pref = prefService.getBranch('');

    if (!pref)
    {
      throw 'JavaScriptOptions: unable to get prefService branch';
    }

    try
    {
      this.orig.strict = this.strict = 
        pref.getBoolPref('javascript.options.strict');
    }
    catch(e)
    {
    }

    try
    {
      this.orig.werror = this.werror = 
        pref.getBoolPref('javascript.options.werror');
    }
    catch(e)
    {
    }
  }
}

JavaScriptOptions.prototype.setOption = 
function (optionName, optionValue)
{
  if (typeof options == 'function')
  {
    // shell
    if (this[optionName] != optionValue)
    {
      options(optionName);
    }
  }
  else if (typeof netscape != 'undefined' && 'security' in netscape)
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences-service;1'];
    if (!preferences)
    {
      throw 'setOption: unable to get @mozilla.org/preferences-service;1';
    }

    var prefService = preferences.
    getService(Components.interfaces.nsIPrefService);

    if (!prefService)
    {
      throw 'setOption: unable to get nsIPrefService';
    }

    var pref = prefService.getBranch('');

    if (!pref)
    {
      throw 'setOption: unable to get prefService branch';
    }

    pref.setBoolPref('javascript.options.' + optionName, optionValue);
  }

  this[optionName] = optionValue;

  return;
}


JavaScriptOptions.prototype.reset = function ()
{
  this.setOption('strict', this.orig.strict);
  this.setOption('werror', this.orig.werror);
}

function compareSource(n, expect, actual)
{
    // compare source
    var expectP = expect.
        replace(/([(){},.:\[\]])/mg, ' $1 ').
        replace(/(\w+)/mg, ' $1 ').
        replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
        replace(/\s+/mg, ' ').
        replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

    var actualP = actual.
        replace(/([(){},.:\[\]])/mg, ' $1 ').
        replace(/(\w+)/mg, ' $1 ').
        replace(/<(\/)? (\w+) (\/)?>/mg, '<$1$2$3>').
        replace(/\s+/mg, ' ').
        replace(/new (\w+)\s*\(\s*\)/mg, 'new $1');

    print('expect:\n' + expectP);
    print('actual:\n' + actualP);

    TEST(n, expectP, actualP);

    // actual must be compilable if expect is?
    try
    {
        var expectCompile = 'No Error';
        var actualCompile;

        eval(expect);
        try
        {
            eval(actual);
            actualCompile = 'No Error';
        }
        catch(ex1)
        {
            actualCompile = ex1 + '';
        }
        TEST(n, expectCompile, actualCompile);
    }
    catch(ex)
    {
    }
}
