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
var statprefix = 'regexp = ';
var statmiddle = ',  string = ';
var statsuffix = ',  match=$';
var ERR_LENGTH = '\nERROR !!! Match arrays have different lengths:';
var ERR_NO_MATCH = '\nregexp FAILED to match anything !!!\n';
var ERR_UNEXP_MATCH = '\nregexp MATCHED when we expected it to fail !!!\n';
var cnExpect = '\nExpect: ';
var cnActual = '\nActual: ';
var cnSingleQuote = "'";
var cnBracketL =  '[';
var cnBracketR =  ']';
var cnNewLine = '\n';



// Compares ouput from multiple applications of RegExp(pattern).exec(string)
function testRegExp(patterns, strings, actualmatches, expectedmatches)
{
  var expectedmatch = [ ];
  var actualmatch = [ ];
  var lExpect = -1;
  var lActual = -1;

  for (var i=0; i != patterns.length; i++)
  {
    actualmatch=actualmatches[i];
    expectedmatch=expectedmatches[i];

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
                        getStatus(i) + ERR_LENGTH +
                        cnExpect + format(expectedmatch) +
                        cnActual + format(actualmatch) + cnNewLine
                       );
          continue;
        }

        // OK, the arrays have same length. Compare them element-by-element -
        for (var j=0; j != lActual; j++)
        {
          reportCompare (expectedmatch[j],  actualmatch[j], getStatus(i, j));
        }
      }
      else //expectedmatch is null - that is, we did not expect a match -
      {
        reportFailure(getStatus(i) + ERR_UNEXP_MATCH);
      }
    }
    else // actualmatch is null
    {
      if (expectedmatch)
      {
        reportFailure(getStatus(i) + ERR_NO_MATCH);
      }
      else // we did not expect a match
      {
        // Being ultra-cautious here. Presumably expectedmatch===actualmatch===null
        reportCompare (expectedmatch, actualmatch, getStatus(i));
      }
    }
  }
}


function getStatus(i,j)
{
  if (j)
  {
    return (statprefix + patterns[i] + statmiddle + quote(strings[i]) + statsuffix + j);
  }
  else
  {
    return (statprefix + patterns[i] + statmiddle + quote(strings[i]));
  }
}


function format(text)
{
  return (cnBracketL + text + cnBracketR);
}


function quote(text)
{
  return (cnSingleQuote + text + cnSingleQuote);
}
