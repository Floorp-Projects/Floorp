/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_10.js
   Section:
   Description:        Determining Instance Relationships

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_10";
var TITLE   = "Determining Instance Relationships";

writeHeaderToLog( SECTION + " "+ TITLE);

function InstanceOf( object, constructor ) {
  return object instanceof constructor;
}
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

var pat = new Engineer();

new TestCase( "InstanceOf( pat, Engineer )",
              true,
              InstanceOf( pat, Engineer ) );

new TestCase( "InstanceOf( pat, WorkerBee )",
              true,
              InstanceOf( pat, WorkerBee ) );

new TestCase( "InstanceOf( pat, Employee )",
              true,
              InstanceOf( pat, Employee ) );

new TestCase( "InstanceOf( pat, Object )",
              true,
              InstanceOf( pat, Object ) );

new TestCase( "InstanceOf( pat, SalesPerson )",
              false,
              InstanceOf ( pat, SalesPerson ) );
test();
