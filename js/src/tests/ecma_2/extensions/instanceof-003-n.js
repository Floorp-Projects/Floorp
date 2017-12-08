/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          instanceof-001.js
 *  ECMA Section:       11.8.6
 *  Description:
 *
 *  RelationalExpression instanceof Identifier
 *
 *  Author:             christine@netscape.com
 *  Date:               2 September 1998
 */
var SECTION = "instanceof-003-n";
var TITLE   = "instanceof"

writeHeaderToLog( SECTION + " "+ TITLE);

function InstanceOf( object_1, object_2, expect ) {

  result = object_1 instanceof object_2;

  new TestCase(
    "(" + object_1 + ") instanceof " + object_2,
    expect,
    result );
}

function Gen3(value) {
  this.value = value;
  this.generation = 3;
  this.toString = new Function ( "return \"(Gen\"+this.generation+\" instance)\"" );
}
Gen3.name = 3;
Gen3.__proto__.toString = new Function( "return \"(\"+this.name+\" object)\"");

function Gen2(value) {
  this.value = value;
  this.generation = 2;
}
Gen2.name = 2;
Gen2.prototype = new Gen3();

function Gen1(value) {
  this.value = value;
  this.generation = 1;
}
Gen1.name = 1;
Gen1.prototype = new Gen2();

function Gen0(value) {
  this.value = value;
  this.generation = 0;
}
Gen0.name = 0;
Gen0.prototype = new Gen1();


function GenA(value) {
  this.value = value;
  this.generation = "A";
  this.toString = new Function ( "return \"(instance of Gen\"+this.generation+\")\"" );

}
GenA.prototype = new Gen0();
GenA.name = "A";

function GenB(value) {
  this.value = value;
  this.generation = "B";
  this.toString = new Function ( "return \"(instance of Gen\"+this.generation+\")\"" );
}
GenB.name = "B"
  GenB.prototype = void 0;

// Identifier is not a function
DESCRIPTION = "Identifier is not a function";

InstanceOf( true, true, "error" );

test();
