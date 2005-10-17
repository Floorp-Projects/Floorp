/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda rginda@netscape.com
 *   Bob Clary bob@bclary.com
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
var callStack = new Array();

function writeLineToLog( string ) {
    print( string + "\n");
}
/*
 * The test driver searches for such a phrase in the test output.
 * If such phrase exists, it will set n as the expected exit code.
 */
function expectExitCode(n)
{

    writeLineToLog('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');

}

/*
 * Statuses current section of a test
 */
function inSection(x)
{

    return SECT_PREFIX + x + SECT_SUFFIX;

}

/*
 * Some tests need to know if we are in Rhino as opposed to SpiderMonkey
 */
function inRhino()
{
    return (typeof defineClass == "function");
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (msg)
{
    var lines = msg.split ("\n");
    var l;
    var funcName = currentFunc();
    var prefix = (funcName) ? "[reported from " + funcName + "] ": "";
    
    for (var i=0; i<lines.length; i++)
        writeLineToLog (FAILED + prefix + lines[i]);

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
        writeLineToLog (STATUS + lines[i]);

}

/*
 * Print a bugnumber message.
 */
function printBugNumber (num)
{

    writeLineToLog (BUGNUMBER + num);

}

/*
 * Compare expected result to actual result, if they differ (in value and/or
 * type) report a failure.  If description is provided, include it in the 
 * failure report.
 */
function reportCompare (expected, actual, description)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
    
    if ((VERBOSE) && (typeof description != "undefined"))
        printStatus ("Comparing '" + description + "'");

    if (expected_t != actual_t)
        output += "Type mismatch, expected type " + expected_t + 
            ", actual type " + actual_t + "\n";
    else if (VERBOSE)
        printStatus ("Expected type '" + actual_t + "' matched actual " +
                     "type '" + expected_t + "'");

    if (expected != actual)
        output += "Expected value '" + expected + "', Actual value '" + actual +
            "'\n";
    else if (VERBOSE)
        printStatus ("Expected value '" + actual + "' matched actual " +
                     "value '" + expected + "'");

    if (output != "")
    {
        if (typeof description != "undefined")
            reportFailure (description);
        reportFailure (output);   
    }
    return (output == ""); // true if passed
}

/*
 * Puts funcName at the top of the call stack.  This stack is used to show
 * a function-reported-from field when reporting failures.
 */
function enterFunc (funcName)
{

    if (!funcName.match(/\(\)$/))
        funcName += "()";

    callStack.push(funcName);

}

/*
 * Pops the top funcName off the call stack.  funcName is optional, and can be
 * used to check push-pop balance.
 */
function exitFunc (funcName)
{
    var lastFunc = callStack.pop();
    
    if (funcName)
    {
        if (!funcName.match(/\(\)$/))
            funcName += "()";

        if (lastFunc != funcName)
            reportFailure ("Test driver failure, expected to exit function '" +
                           funcName + "' but '" + lastFunc + "' came off " +
                           "the stack");
    }
    
}

/*
 * Peeks at the top of the call stack.
 */
function currentFunc()
{
    
    return callStack[callStack.length - 1];
    
}

/*
  Calculate the "order" of a set of data points {X: [], Y: []}
  by computing successive "derivatives" of the data until
  the data is exhausted or the derivative is linear.
*/
function BigO(data)
{
  var order = 0;
  var origLength = data.X.length;

  while (data.X.length > 1)
  {
    var lr = new LinearRegression(data);
    order++;
    if (lr.SE < 0.01)
    {
      break;
    }
    data = dataDeriv(data);
  }
 
  if (order == origLength - 1)
  {
    order = Number.POSITIVE_INFINITY;
  }
  return order;

  function LinearRegression(data)
    {
      // http://www.bearcave.com/misl/misl_tech/wavelets/stat/
      /*
        X0,X1...; Y0,Y1,...
        b = sum( (Xi - Xavg)(Yi - Yavg) ) / sum ( (Xi - Xavg)*(Xi - Xavg) )
        a = Yavg - b * Xavg
        stddev*stddev = (sum( (Yi - Yavg) * (Yi - Yavg) ) - 
        b * sum( (Xi - Xavg)*(Yi - Yavg) ) ) /
        sum ( (Xi - Xavg) * (Xi - Xavg) )
      */
      var i;

      if (data.X.length != data.Y.length)
      {
        throw 'LinearRegression: data point length mismatch';
      }
      if (data.X.length < 2)
      {
        throw 'LinearRegression: data point length < 2';
      }
      var length = data.X.length;
      var X = data.X;
      var Y = data.Y;

      this.Xavg = 0;
      this.Yavg = 0;

      for (i = 0; i < length; i++)
      {
        this.Xavg += X[i];
        this.Yavg += Y[i];
      }

      var sumXdiffYdiff = 0;
      var sumXdiffsquared = 0;
      var sumYdiffsquared = 0;

      for (i = 0; i < length; i++)
      {
        var xDiff = (X[i] - this.Xavg);
        var yDiff = (Y[i] - this.Yavg);

        sumXdiffYdiff   += xDiff * yDiff;
        sumXdiffsquared += xDiff * xDiff; 
        sumYdiffsquared += yDiff * yDiff;
      }

      this.b = sumXdiffYdiff / sumXdiffsquared;
      this.a = this.Yavg - this.b * this.Xavg;
      // standard deviation of regression
      this.sigma = Math.sqrt( (sumYdiffsquared - this.b * sumXdiffYdiff) / 
                              (length - 2));
      // standard error in b
      this.SE = this.sigma / Math.sqrt(sumXdiffsquared);
    }

  function dataDeriv(data)
    {
      if (data.X.length != data.Y.length)
      {
        throw 'length mismatch';
      }
      var length = data.X.length;

      if (length < 2)
      {
        throw 'length ' + length + ' must be >= 2';
      }
      var X = data.X;
      var Y = data.Y;

      var deriv = {X: [], Y: [] };

      for (var i = 0; i < length - 1; i++)
      {
        deriv.X[i] = (X[i] + X[i+1])/2;
        deriv.Y[i] = (Y[i+1] - Y[i])/(X[i+1] - X[i]);
      }  
      return deriv;
    }

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

  this.privileges = 'UniversalXPConnect';

  if (typeof options == 'function')
  {
    // shell
    var optString = options();
    if (optString)
    {
      var optList = optString.split(',');
      for (iOpt = 0; i < optList.length; iOpt++)
      {
        optName = optList[iOpt];
        this[optName] = true;
      }
    }
  }
  else if (typeof document != 'undefined')
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences;1'];
    if (!preferences)
    {
      throw 'JavaScriptOptions: unable to get @mozilla.org/preference;1';
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

    this.orig.strict = this.strict = pref.getBoolPref('javascript.options.strict');
    this.orig.werror = this.werror = pref.getBoolPref('javascript.options.werror');
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
  else if (typeof document != 'undefined')
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences;1'];
    if (!preferences)
    {
      throw 'setOption: unable to get @mozilla.org/preference;1';
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
