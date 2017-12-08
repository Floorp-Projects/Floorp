/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 07 February 2001
 *
 * Functionality common to Array testing -
 */
//-----------------------------------------------------------------------------


var CHAR_LBRACKET = '[';
var CHAR_RBRACKET = ']';
var CHAR_QT_DBL = '"';
var CHAR_QT = "'";
var CHAR_NL = '\n';
var CHAR_COMMA = ',';
var CHAR_SPACE = ' ';
var TYPE_STRING = typeof 'abc';


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
 * But toSource() doesn't exist in Rhino, so use our own imitation, below -
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
    return toSource(arr);
  }
}



/*
 * Imitate SpiderMonkey's arr.toSource() method:
 *
 * a) Double-quote each array element that is of string type
 * b) Represent |undefined| and |null| by empty strings
 * c) Delimit elements by a comma + single space
 * d) Do not add delimiter at the end UNLESS the last element is |undefined|
 * e) Add square brackets to the beginning and end of the string
 */
function toSource(arr)
{
  var delim = CHAR_COMMA + CHAR_SPACE;
  var elt = '';
  var ret = '';
  var len = arr.length;

  for (i=0; i<len; i++)
  {
    elt = arr[i];

    switch(true)
    {
    case (typeof elt === TYPE_STRING) :
      ret += doubleQuote(elt);
      break;

    case (elt === undefined || elt === null) :
      break; // add nothing but the delimiter, below -

    default:
      ret += elt.toString();
    }

    if ((i < len-1) || (elt === undefined))
      ret += delim;
  }

  return  CHAR_LBRACKET + ret + CHAR_RBRACKET;
}


function doubleQuote(text)
{
  return CHAR_QT_DBL + text + CHAR_QT_DBL;
}


function singleQuote(text)
{
  return CHAR_QT + text + CHAR_QT;
}

