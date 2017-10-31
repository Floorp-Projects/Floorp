/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_11.js
   Section:
   Description:        Global Information in Constructors

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   This tests the syntax ObjectName.prototype = new PrototypeObject using the
   Employee example in the document referenced above.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_11";
var TITLE   = "Global Information in Constructors";

writeHeaderToLog( SECTION + " "+ TITLE);

var idCounter = 1;


function Employee ( name, dept ) {
  this.name = name || "";
  this.dept = dept || "general";
  this.id = idCounter++;
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

var pat = new Employee( "Toonces, Pat", "Tech Pubs" )
  var terry = new Employee( "O'Sherry Terry", "Marketing" );

var les = new Engineer( "Morris, Les",  new Array("JavaScript"), "indy" );

new TestCase( "pat.id",
	      5,
	      pat.id );

new TestCase( "terry.id",
	      6,
	      terry.id );

new TestCase( "les.id",
	      7,
	      les.id );


test();

