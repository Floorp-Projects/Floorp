/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_1.js
   Section:
   Description:        new PrototypeObject

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_1";
var TITLE   = "new PrototypeObject";

writeHeaderToLog( SECTION + " "+ TITLE);

function Employee () {
  this.name = "";
  this.dept = "general";
}
function Manager () {
  this.reports = [];
}
Manager.prototype = new Employee();

function WorkerBee () {
  this.projects = new Array();
}
WorkerBee.prototype = new Employee();

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee();

function Engineer () {
  this.dept = "engineering";
  this.machine = "";
}
Engineer.prototype = new WorkerBee();

var jim = new Employee();

new TestCase( "jim = new Employee(); jim.name",
	      "",
	      jim.name );


new TestCase( "jim = new Employee(); jim.dept",
	      "general",
	      jim.dept );

var sally = new Manager();

new TestCase( "sally = new Manager(); sally.name",
	      "",
	      sally.name );
new TestCase( "sally = new Manager(); sally.dept",
	      "general",
	      sally.dept );

new TestCase( "sally = new Manager(); sally.reports.length",
	      0,
	      sally.reports.length );

new TestCase( "sally = new Manager(); typeof sally.reports",
	      "object",
	      typeof sally.reports );

var fred = new SalesPerson();

new TestCase( "fred = new SalesPerson(); fred.name",
	      "",
	      fred.name );

new TestCase( "fred = new SalesPerson(); fred.dept",
	      "sales",
	      fred.dept );

new TestCase( "fred = new SalesPerson(); fred.quota",
	      100,
	      fred.quota );

new TestCase( "fred = new SalesPerson(); fred.projects.length",
	      0,
	      fred.projects.length );

var jane = new Engineer();

new TestCase( "jane = new Engineer(); jane.name",
	      "",
	      jane.name );

new TestCase( "jane = new Engineer(); jane.dept",
	      "engineering",
	      jane.dept );

new TestCase( "jane = new Engineer(); jane.projects.length",
	      0,
	      jane.projects.length );

new TestCase( "jane = new Engineer(); jane.machine",
	      "",
	      jane.machine );


test();
