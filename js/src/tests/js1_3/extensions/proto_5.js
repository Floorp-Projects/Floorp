/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_5.js
   Section:
   Description:        Logical OR || in Constructors

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   This tests the logical OR opererator || syntax in constructors.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_5";
var TITLE   = "Logical OR || in Constructors";

writeHeaderToLog( SECTION + " "+ TITLE);

function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
}
function Manager () {
  this.reports = [];
}
Manager.prototype = new Employee();

function WorkerBee ( projs ) {
  this.projects = projs || new Array();
}
WorkerBee.prototype = new Employee();

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee();

function Engineer ( machine ) {
  this.dept = "engineering";
  this.machine = machine || "";
}
Engineer.prototype = new WorkerBee();


var pat = new Engineer( "indy" );

var les = new Engineer();

new TestCase( "var pat = new Engineer(\"indy\"); pat.name",
	      "",
	      pat.name );

new TestCase( "pat.dept",
	      "engineering",
	      pat.dept );

new TestCase( "pat.projects.length",
	      0,
	      pat.projects.length );

new TestCase( "pat.machine",
	      "indy",
	      pat.machine );

new TestCase( "pat.__proto__ == Engineer.prototype",
	      true,
	      pat.__proto__ == Engineer.prototype );

new TestCase( "var les = new Engineer(); les.name",
	      "",
	      les.name );

new TestCase( "les.dept",
	      "engineering",
	      les.dept );

new TestCase( "les.projects.length",
	      0,
	      les.projects.length );

new TestCase( "les.machine",
	      "",
	      les.machine );

new TestCase( "les.__proto__ == Engineer.prototype",
	      true,
	      les.__proto__ == Engineer.prototype );


test();
