/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_2.js
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

var SECTION = "proto_2";
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

WorkerBee.prototype = new Employee;

function SalesPerson () {
  this.dept = "sales";
  this.quota = 100;
}
SalesPerson.prototype = new WorkerBee;

function Engineer () {
  this.dept = "engineering";
  this.machine = "";
}
Engineer.prototype = new WorkerBee;


var employee    = new Employee();
var manager     = new Manager();
var workerbee   = new WorkerBee();
var salesperson = new SalesPerson();
var engineer    = new Engineer();

new TestCase( "employee.__proto__ == Employee.prototype",
	      true,
	      employee.__proto__ == Employee.prototype );

new TestCase( "manager.__proto__ == Manager.prototype",
	      true,
	      manager.__proto__ == Manager.prototype );

new TestCase( "workerbee.__proto__ == WorkerBee.prototype",
	      true,
	      workerbee.__proto__ == WorkerBee.prototype );

new TestCase( "salesperson.__proto__ == SalesPerson.prototype",
	      true,
	      salesperson.__proto__ == SalesPerson.prototype );

new TestCase( "engineer.__proto__ == Engineer.prototype",
	      true,
	      engineer.__proto__ == Engineer.prototype );

test();

