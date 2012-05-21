// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.2.2-1.js
   ECMA Section:       10.2.2 Eval Code
   Description:

   When control enters an execution context for eval code, the previous
   active execution context, referred to as the calling context, is used to
   determine the scope chain, the variable object, and the this value. If
   there is no calling context, then initializing the scope chain, variable
   instantiation, and determination of the this value are performed just as
   for global code.

   The scope chain is initialized to contain the same objects, in the same
   order, as the calling context's scope chain.  This includes objects added
   to the calling context's scope chain by WithStatement.

   Variable instantiation is performed using the calling context's variable
   object and using empty property attributes.

   The this value is the same as the this value of the calling context.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.2.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Eval Code";

writeHeaderToLog( SECTION + " "+ TITLE);

var THIS = eval("this");

new TestCase( SECTION,
	      "this +''",
	      GLOBAL,
	      THIS + "" );

var GLOBAL_PROPERTIES = new Array();
var i = 0;

for ( p in THIS ) {
  GLOBAL_PROPERTIES[i++] = p;
}

for ( i = 0; i < GLOBAL_PROPERTIES.length; i++ ) {
  new TestCase( SECTION,
		GLOBAL_PROPERTIES[i] +" == THIS["+GLOBAL_PROPERTIES[i]+"]",
		true,
		eval(GLOBAL_PROPERTIES[i]) == eval( "THIS[GLOBAL_PROPERTIES[i]]") );
}

//  this in eval statements is the same as this value of the calling context

var RESULT = THIS == this;

new TestCase( SECTION,
	      "eval( 'this == THIS' )",
	      true,
	      RESULT );

var RESULT = THIS +'';

new TestCase( SECTION,
	      "eval( 'this + \"\"' )",
	      GLOBAL,
	      RESULT );


new TestCase( SECTION,
	      "eval( 'this == THIS' )",
	      true,
	      eval( "this == THIS" ) );

new TestCase( SECTION,
	      "eval( 'this + \"\"' )",
	      GLOBAL,
	      eval( "this +''") );


test();
