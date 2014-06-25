/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.12.js
   ECMA Section:       11.12 Conditional Operator
   Description:
   Logi

   calORExpression ? AssignmentExpression : AssignmentExpression

   Semantics

   The production ConditionalExpression :
   LogicalORExpression ? AssignmentExpression : AssignmentExpression
   is evaluated as follows:

   1.  Evaluate LogicalORExpression.
   2.  Call GetValue(Result(1)).
   3.  Call ToBoolean(Result(2)).
   4.  If Result(3) is false, go to step 8.
   5.  Evaluate the first AssignmentExpression.
   6.  Call GetValue(Result(5)).
   7.  Return Result(6).
   8.  Evaluate the second AssignmentExpression.
   9.  Call GetValue(Result(8)).
   10.  Return Result(9).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.12";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Conditional operator( ? : )");

new TestCase( SECTION,   
              "true ? 'PASSED' : 'FAILED'",    
              "PASSED",      
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "false ? 'FAILED' : 'PASSED'",    
              "PASSED",     
              (false?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "1 ? 'PASSED' : 'FAILED'",    
              "PASSED",         
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "0 ? 'FAILED' : 'PASSED'",    
              "PASSED",         
              (false?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "-1 ? 'PASSED' : 'FAILED'",    
              "PASSED",         
              (true?"PASSED":"FAILED"));

new TestCase( SECTION,   
              "NaN ? 'FAILED' : 'PASSED'",    
              "PASSED",         
              (Number.NaN?"FAILED":"PASSED"));

new TestCase( SECTION,   
              "var VAR = true ? , : 'FAILED'",
              "PASSED",          
              (VAR = true ? "PASSED" : "FAILED") );

test();
