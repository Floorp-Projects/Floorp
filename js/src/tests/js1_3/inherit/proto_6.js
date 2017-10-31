/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_6.js
   Section:
   Description:        Logical OR || in constructors

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

var SECTION = "proto_6";
var TITLE   = "Logical OR || in constructors";

writeHeaderToLog( SECTION + " "+ TITLE);

function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
}
function Manager () {
  this.reports = [];
}
Manager.prototype = new Employee();

function WorkerBee ( name, dept, projs ) {
  this.base = Employee;
  this.base( name, dept)
    this.projects = projs || new Array();
}

WorkerBee.prototype = new Employee();

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee();

function Engineer ( name, projs, machine ) {
  this.base = WorkerBee;
  this.base( name, "engineering", projs )
    this.machine = machine || "";
}
Engineer.prototype = new WorkerBee();

var pat = new Engineer( "Toonces, Pat",
			["SpiderMonkey", "Rhino"],
			"indy" );

var les = new WorkerBee( "Morris, Les",
			 "Training",
			 ["Hippo"] )

  var terry = new Employee( "Boomberi, Terry",
			    "Marketing" );

// Pat, the Engineer

new TestCase( "pat.name",
	      "Toonces, Pat",
	      pat.name );

new TestCase( "pat.dept",
	      "engineering",
	      pat.dept );

new TestCase( "pat.projects.length",
	      2,
	      pat.projects.length );

new TestCase( "pat.projects[0]",
	      "SpiderMonkey",
	      pat.projects[0] );

new TestCase( "pat.projects[1]",
	      "Rhino",
	      pat.projects[1] );

new TestCase( "pat.machine",
	      "indy",
	      pat.machine );


// Les, the WorkerBee

new TestCase( "les.name",
	      "Morris, Les",
	      les.name );

new TestCase( "les.dept",
	      "Training",
	      les.dept );

new TestCase( "les.projects.length",
	      1,
	      les.projects.length );

new TestCase( "les.projects[0]",
	      "Hippo",
	      les.projects[0] );

// Terry, the Employee
new TestCase( "terry.name",
	      "Boomberi, Terry",
	      terry.name );

new TestCase( "terry.dept",
	      "Marketing",
	      terry.dept );
test();

