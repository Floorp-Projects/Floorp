/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS  IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 07 February 2001
*
* Functionality common to RegExp testing -
*/
//-------------------------------------------------------------------------------------------------
var MSG_PATTERN = '\nregexp = ';
var MSG_STRING = '\nstring = ';
var MSG_EXPECT = '\nExpect: ';
var MSG_ACTUAL = '\nActual: ';
var ERR_LENGTH = '\nERROR !!! match arrays have different lengths:';
var ERR_MATCH = '\nERROR !!! regexp failed to give expected match array:';
var ERR_NO_MATCH = '\nERROR !!! regexp FAILED to match anything !!!';
var ERR_UNEXP_MATCH = '\nERROR !!! regexp MATCHED when we expected it to fail !!!';
var CHAR_LBRACKET = '[';
var CHAR_RBRACKET = ']';
var CHAR_QT = "'";
var CHAR_NL = '\n';


function testRegExp(statuses, patterns, strings, actualmatches, expectedmatches)
{
  var status = '';
  var pattern = new RegExp();
  var string = '';
  var actualmatch = new Array();
  var expectedmatch = new Array();
  var state = '';
  var lActual = -1;
  var lExpect = -1;


  for (var i=0; i != patterns.length; i++)
  {
    status = statuses[i];
    pattern = patterns[i];
    string = strings[i];
    actualmatch=actualmatches[i];
    expectedmatch=expectedmatches[i];
    state = getState(status, pattern, string);


    if(actualmatch)
    {
      if(expectedmatch)
      {
        // expectedmatch and actualmatch are arrays -
        lExpect = expectedmatch.length;
        lActual = actualmatch.length;
 
        if (lActual != lExpect)
        {
          reportFailure(
                        state + ERR_LENGTH +
                        MSG_EXPECT + formatArray(expectedmatch) +
                        MSG_ACTUAL + formatArray(actualmatch) +
                        CHAR_NL
                       );
          continue;
        }

        // OK, the arrays have same length -
        if (formatArray(expectedmatch) != formatArray(actualmatch))
        {
          reportFailure(
                        state + ERR_MATCH +
                        MSG_EXPECT + formatArray(expectedmatch) +
                        MSG_ACTUAL + formatArray(actualmatch) +
                        CHAR_NL
                       );
        }

      }
      else //expectedmatch is null - that is, we did not expect a match -
      {
        reportFailure(
                      state + ERR_UNEXP_MATCH +
                      MSG_EXPECT + expectedmatch +
                      MSG_ACTUAL + formatArray(actualmatch) +
                      CHAR_NL
                     );
      }

    }
    else // actualmatch is null
    {
      if (expectedmatch)
      {
        reportFailure(
                      state + ERR_NO_MATCH +
                      MSG_EXPECT + formatArray(expectedmatch) +
                      MSG_ACTUAL + actualmatch +
                      CHAR_NL
                     );
      }
      else // we did not expect a match
      {
        // Being ultra-cautious. Presumably expectedmatch===actualmatch===null
        reportCompare (expectedmatch, actualmatch, state);
      }
    }
  }
}


function getState(status, pattern, string)
{
  /*
   * Escape \n's, etc. to make them LITERAL in the presentation string.
   * We don't have to worry about this in |pattern|; such escaping is
   * done automatically by pattern.toString(), invoked implicitly below.
   *
   * One would like to simply do: string = string.replace(/\s/g, '\$1').
   * However, the backreference $1 is not a literal string value, 
   * so this method doesn't work.
   *
   * Also tried string = string.replace(/\s/g, escape('$1'));
   * but such a presentation is in hexadecimal form...
   */
  string = string.replace(/\n/g, '\\n');
  string = string.replace(/\r/g, '\\r');
  string = string.replace(/\t/g, '\\t');
  string = string.replace(/\v/g, '\\v');
  string = string.replace(/\f/g, '\\f');

  return (status + MSG_PATTERN + pattern + MSG_STRING + quote(string));
}


/*
 * If available, arr.toSource() gives more detail than arr.toString()
 *
 * var arr = Array(1,2,'3');
 *
 * arr.toSource()
 * [1, 2, "3"]
 *
 * arr.toString()
 * 1,2,3
 *
 * But toSource() doesn't exist in Rhino - so branch on this -
 *
 */
function formatArray(arr)
{
  try
  {
    return arr.toSource();
  }
  catch(e)
  {
    return CHAR_LBRACKET + arr.toString() + CHAR_RBRACKET;
  }
}


function quote(text)
{
  return (CHAR_QT + text + CHAR_QT);
}
