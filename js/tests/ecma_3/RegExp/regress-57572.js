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
* Date: 28 December 2000
*
*
* SUMMARY: Testing regular expressions containing the ? character.
* Arose from Bugzilla bug 57572: "RegExp with ? matches incorrectly"
*
*/
//-------------------------------------------------------------------------------------------------
var bug = 57572;
var summary = 'Testing regular expressions containing "?"';
var statprefix = 'regexp = '; var statmiddle =  ', string = '; var statsuffix = ', $=';
var cnSingleQuote = "'"; var cnEmptyString = ''; var cnSingleSpace = ' ';
var i = -1; var j = -1;
var pattern = ''; var patterns = new Array();
var string = ''; var strings = new Array();
var actual = '';  var actuals = new Array();
var expect = ''; expects = new Array();


pattern = /(\S+)?(.*)/; 
string = 'Test this';
actual = string.match(pattern);
expect = Array(string,  'Test',  ' this');  //single space in front of  'this'
addThis();

pattern = /(\S+)? ?(.*)/;  //single space between the ? characters
string= 'Test this';
actual = string.match(pattern);
expect = Array(string,  'Test',  'this');  //NO space in front of  'this'
addThis(); 

pattern = /(\S+)????(.*)/;   
string= 'Test this';
actual = string.match(pattern);
expect = Array(string, cnEmptyString, string);
addThis(); 

pattern = /(\S+)?(.*)/; 
string = 'Stupid phrase, with six - (short) words';
actual = string.match(pattern);
expect = Array(string,  'Stupid',  ' phrase, with six - (short) words');  //single space in front of 'phrase'
addThis(); 

pattern = /(\S+)? ?(.*)/;  //single space between the ? characters
string = 'Stupid phrase, with six - (short) words';
actual = string.match(pattern);
expect = Array(string,  'Stupid',  'phrase, with six - (short) words');  //NO space in front of 'phrase'
addThis(); 

// let's add an extra back-reference this time - three instead of two -
pattern = /(\S+)?( ?)(.*)/;  //single space before second ? character
string = 'Stupid phrase, with six - (short) words';
actual = string.match(pattern);
expect = Array(string,  'Stupid', cnSingleSpace, 'phrase, with six - (short) words'); 
addThis(); 

pattern = /^(\S+)?( ?)(B+)$/;  //single space before second ? character
string = 'AAABBB';
actual = string.match(pattern);
expect = Array(string,  'AAABB', cnEmptyString,  'B');  
addThis(); 

pattern = /(\S+)?(!?)(.*)/;
string = 'WOW !!! !!!';
actual = string.match(pattern);
expect = Array(string, 'WOW', cnEmptyString,  ' !!! !!!');
addThis(); 

pattern = /(.+)?(!?)(!+)/;
string = 'WOW !!! !!!';
actual = string.match(pattern);
expect = Array(string, 'WOW !!! !!', cnEmptyString,  '!');
addThis(); 



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------



function addThis() 
{
  i++;
  patterns[i] = pattern;
  strings[i] = string;
  actuals[i] = actual;
  expects[i] = expect;
}


function test() 
{
  enterFunc ('test'); 
  printBugNumber (bug);
  printStatus (summary);
  
  for (i=0; i != patterns.length; i++)
  {
    // expects[i] and actuals[i] are array objects. Compare them element-by-element -
    for (j=0; j !=actuals[i].length; j++)
    {
      reportCompare (expects[i][j], actuals[i][j], getStatus(i, j));
    }
  }
 
  exitFunc ('test');
}


function getStatus(i, j)
{
  return (statprefix  +  quote(patterns[i])  +  statmiddle  +  quote(strings[i])  +  statsuffix  +  j ); 
}


function quote(text)
{
  return (cnSingleQuote  +  text  +  cnSingleQuote);
}