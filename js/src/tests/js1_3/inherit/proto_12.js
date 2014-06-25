/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          proto_12.js
   Section:
   Description:        new PrototypeObject

   This tests Object Hierarchy and Inheritance, as described in the document
   Object Hierarchy and Inheritance in JavaScript, last modified on 12/18/97
   15:19:34 on http://devedge.netscape.com/.  Current URL:
   http://devedge.netscape.com/docs/manuals/communicator/jsobj/contents.htm

   No Multiple Inheritance

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "proto_12";
var VERSION = "JS1_3";
var TITLE   = "No Multiple Inheritance";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

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

function Hobbyist( hobby ) {
  this.hobby = hobby || "yodeling";
}

function Engineer ( name, projs, machine, hobby ) {
  this.base1 = WorkerBee;
  this.base1( name, "engineering", projs )

    this.base2 = Hobbyist;
  this.base2( hobby );

  this.projects = projs || new Array();
  this.machine = machine || "";
}
Engineer.prototype = new WorkerBee();

var idCounter = 1;

var les = new Engineer( "Morris, Les",  new Array("JavaScript"), "indy" );

Hobbyist.prototype.equipment = [ "horn", "mountain", "goat" ];

new TestCase( SECTION,
	      "les.name",
	      "Morris, Les",
	      les.name );

new TestCase( SECTION,
	      "les.dept",
	      "engineering",
	      les.dept );

Array.prototype.getClass = Object.prototype.toString;

new TestCase( SECTION,
	      "les.projects.getClass()",
	      "[object Array]",
	      les.projects.getClass() );

new TestCase( SECTION,
	      "les.projects[0]",
	      "JavaScript",
	      les.projects[0] );

new TestCase( SECTION,
	      "les.machine",
	      "indy",
	      les.machine );

new TestCase( SECTION,
	      "les.hobby",
	      "yodeling",
	      les.hobby );

new TestCase( SECTION,
	      "les.equpment",
	      void 0,
	      les.equipment );

test();
