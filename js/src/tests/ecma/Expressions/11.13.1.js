/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.13.1.js
   ECMA Section:       11.13.1 Simple assignment
   Description:

   11.13.1 Simple Assignment ( = )

   The production AssignmentExpression :
   LeftHandSideExpression = AssignmentExpression is evaluated as follows:

   1.  Evaluate LeftHandSideExpression.
   2.  Evaluate AssignmentExpression.
   3.  Call GetValue(Result(2)).
   4.  Call PutValue(Result(1), Result(3)).
   5.  Return Result(3).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.13.1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Simple Assignment ( = )");

new TestCase( SECTION,   
              "SOMEVAR = true",    
              true,  
              SOMEVAR = true );

test();

