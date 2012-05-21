/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 418641;
var summary = '++ and -- correctness';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function get_pre_check(operand, op)
{
  return "{\n"+
    "    "+operand+" = I;\n"+
    "    let tmp = "+op+op+operand+";\n"+
    "    if ("+operand+" !== Number(I) "+op+" 1)\n"+
    "        throw Error('"+op+op+operand+" case 1 for '+uneval(I));\n"+
    "    if (tmp !== "+operand+")\n"+
    "        throw Error('"+op+op+operand+" case 2 for '+uneval(I));\n"+
    "}\n";    
}

function get_post_check(operand, op)
{
  return "{\n"+
    "    "+operand+" = I;\n"+
    "    let tmp = "+operand+op+op+";\n"+
    "    if ("+operand+" !== Number(I) "+op+" 1)\n"+
    "        throw Error('"+operand+op+op+" case 1 for '+uneval(I));\n"+
    "    if (tmp !== Number(I))\n"+
    "        throw Error('"+op+op+operand+" case 2 for '+uneval(I));\n"+
    "}\n";    
}

function get_check_source(operand)
{
  return get_pre_check(operand, '+')+
    get_pre_check(operand, '-')+
    get_post_check(operand, '+')+
    get_post_check(operand, '-');
}

var arg_check = Function('I', 'a', get_check_source('a'));
var let_check = Function('I', 'let a;'+get_check_source('a'));
var var_check = Function('I', 'var a;'+get_check_source('a'));

var my_name;
var my_obj = {};
var my_index = 0;
var name_check = Function('I', get_check_source('my_name'));
var prop_check = Function('I', get_check_source('my_obj.x'));
var elem_check = Function('I', get_check_source('my_obj[my_index]'));

var test_values = [0 , 0.5, -0.0, (1 << 30) - 1, 1 - (1 << 30)];

for (let i = 0; i != test_values.length; i = i + 1) {
  let x = [test_values[i], String(test_values[i])];
  for (let j = 0; j != x.length; j = j + 1) {
    try
    {
      expect = actual = 'No Error';
      let test_value = x[j];
      arg_check(test_value, 0);
      let_check(test_value);
      var_check(test_value);
      name_check(test_value);
      prop_check(test_value);
      elem_check(test_value);
    }
    catch(ex)
    {
      actual = ex + '';
    }
    reportCompare(expect, actual, summary + ': ' + i + ', ' + j);
  }
}
