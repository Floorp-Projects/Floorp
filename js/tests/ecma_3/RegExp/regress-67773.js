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
* Date: 06 February 2001
*
* SUMMARY:  Arose from Bugzilla bug 67773: 
* "Regular subexpressions followed by + failing to run to completion"
*
*  See http://bugzilla.mozilla.org/show_bug.cgi?id=67773
*/
//-------------------------------------------------------------------------------------------------
var bug = 67773;
var summary = 'Testing regular subexpressions followed by ? or +';
var statprefix = 'regexp = '; 
var statmiddle =  ',  string = '; 
var statsuffix = ',  match=$';
var cnSingleQuote = "'"; var cnEmptyString = ''; var cnSingleSpace = ' ';
var cnNoMatch = 'regexp FAILED to match anything!!!\n';
var cnUnexpectedMatch = 'regexp MATCHED when we expected it to fail!!!\n';
var i = -1; var j = -1;
var pattern = ''; var patterns = new Array();
var string = ''; var strings = new Array();
var actualmatch = '';  var actual = new Array();
var expectedmatch = ''; var expect = new Array();


pattern = /^(\S+)?( ?)(B+)$/;  //single space before second ? character
    string = 'AAABBB AAABBB ';  //single space at middle and at end -
    actualmatch = string.match(pattern);
    expectedmatch = null;  
    addThis(); 

    string = 'AAABBB BBB';  //single space in the middle
    actualmatch = string.match(pattern);
    expectedmatch = Array(string,  'AAABBB', cnSingleSpace,  'BBB');  
    addThis(); 

    string = 'AAABBB AAABBB';  //single space in the middle
    actualmatch = string.match(pattern);
    expectedmatch = null;  
    addThis(); 


pattern = /^(A+B)+$/;
    string = 'AABAAB';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string,  'AAB');  
    addThis();

    string = 'ABAABAAAAAAB';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string,  'AAAAAAB');  
    addThis();

    string = 'ABAABAABAB';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string,  'AB');  
    addThis(); 

    string = 'ABAABAABABB';
    actualmatch = string.match(pattern);
    expectedmatch = null;   // because string doesn't match at end   
    addThis(); 


pattern = /^(A+1)+$/;
    string = 'AA1AA1';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string,  'AA1');  
    addThis(); 


pattern = /^(\w+\-)+$/;
    string = '';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, string);  
    addThis(); 

    string = 'bla-';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, string);  
    addThis();

    string = 'bla-bla';  // hyphen missing at end - 
    actualmatch = string.match(pattern);
    expectedmatch = null;  //because string doesn't match at end
    addThis(); 

    string = 'bla-bla-';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, 'bla-');  
    addThis(); 


pattern = /^(\S+)+(A+)$/;
    string = 'asdlfkjasdflkjAAA';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, 'asdlfkjasdflkjAA', 'A');  
    addThis(); 

    string = 'asdlfkjasdflkj AAA'; // space in middle
    actualmatch = string.match(pattern);
    expectedmatch = null;  //because of the space
    addThis(); 


pattern = /^(\S+)+(\d+)$/;
    string = 'asdlfkjasdflkj122211';
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, 'asdlfkjasdflkj12221', '1');  
    addThis(); 

    string = 'asdlfkjasdflkj1111111aaaaa1'; 
    actualmatch = string.match(pattern);
    expectedmatch = Array(string, 'asdlfkjasdflkj1111111aaaaa', '1'); 
    addThis(); 



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------



function addThis() 
{
  i++;
  patterns[i] = pattern;
  strings[i] = string;
  actual[i] = actualmatch;
  expect[i] = expectedmatch;
}


function test() 
{
  enterFunc ('test'); 
  printBugNumber (bug);
  printStatus (summary);
  
  for (i=0; i != patterns.length; i++)
  {
    actualmatch=actual[i];
    expectedmatch=expect[i];

    if(actualmatch) // there was a successful match - 
    {
      if(expectedmatch)
      {
        // expectedmatch and actualmatch are array objects. Compare them element-by-element -
        for (j=0; j !=actualmatch.length; j++)
        {
          reportCompare (expectedmatch[j],  actualmatch[j], getStatus(i, j));
        }
      }
      else // we did not expect a match -
      {
        reportFailure(cnUnexpectedMatch + getStatus(i));
      }    
    }
    else // actualmatch is null; there was no successful match - 
    {
      if (expectedmatch)  // we expected a match
      {    
        reportFailure(cnNoMatch + getStatus(i));
      }
      else // we did not expect a match
      {
        // Being ultra-cautious here. Presumably expectedmatch===actualmatch===null
        reportCompare (expectedmatch, actualmatch, getStatus(i));
      }
    }
  }
 
  exitFunc ('test');
}


function getStatus(i, j)
{
  if ( j )
  {
    return (statprefix  +  patterns[i]  +  statmiddle  +  quote(strings[i])  +  statsuffix  +  j ); 
  }
  else
  {
    return (statprefix  +  patterns[i]  +  statmiddle  +  quote(strings[i]));
  }
}


function quote(text)
{
  return (cnSingleQuote  +  text  +  cnSingleQuote);
}
