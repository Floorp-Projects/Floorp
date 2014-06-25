/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_7.js
   Section:
   Description:        Adding Properties to the Prototype Object

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   This tests

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_6";
var VERSION = "JS1_3";
var TITLE   = "Adding properties to the Prototype Object";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
}
function WorkerBee ( name, dept, projs ) {
  this.base = Employee;
  this.base( name, dept)
    this.projects = projs || new Array();
}
WorkerBee.prototype = new Employee();

function Engineer ( name, projs, machine ) {
  this.base = WorkerBee;
  this.base( name, "engineering", projs )
    this.machine = machine || "";
}
// Engineer.prototype = new WorkerBee();

var pat = new Engineer( "Toonces, Pat",
			["SpiderMonkey", "Rhino"],
			"indy" );

Employee.prototype.specialty = "none";


// Pat, the Engineer

new TestCase( SECTION,
	      "pat.name",
	      "Toonces, Pat",
	      pat.name );

new TestCase( SECTION,
	      "pat.dept",
	      "engineering",
	      pat.dept );

new TestCase( SECTION,
	      "pat.projects.length",
	      2,
	      pat.projects.length );

new TestCase( SECTION,
	      "pat.projects[0]",
	      "SpiderMonkey",
	      pat.projects[0] );

new TestCase( SECTION,
	      "pat.projects[1]",
	      "Rhino",
	      pat.projects[1] );

new TestCase( SECTION,
	      "pat.machine",
	      "indy",
	      pat.machine );

new TestCase( SECTION,
	      "pat.specialty",
	      void 0,
	      pat.specialty );

test();
