/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * Date: 2001-07-23
 *
 * SUMMARY: Testing class inheritance
 *
 */
//-------------------------------------------------------------------------------------------------
var UBound:Integer = 0;
var bug:String = '(none)';
var summary:String = 'Testing class inheritance';
var propNAME:String = 'name';
var propMOTHER:String = 'mother';
var propFATHER:String = 'father';
var propNONEXIST:String = '@#$%@#^';
var nameUNINIT:String = '(NO NAME HAS BEEN ASSIGNED)';
var nameMOTHER:String = 'Bessie';
var nameFATHER:String = 'Red';
var nameCHILD:String = 'Junior';
var status:String = '';
var statusitems:Array = [];
var actual:String = '';
var actualvalues:Array = [];
var expect:String= '';
var expectedvalues:Array = [];


class Cow
{
  static var count:Integer = 0;
  var name:String = nameUNINIT;

  constructor function Cow(sName:String)
  {
    count++;
    name = sName;
  }
}

class Calf extends Cow
{
  var mother:Cow;
  var father:Cow;

  constructor function Calf(sName:String, objMother:Cow, objFather:Cow)
  {
    Cow(sName);
    mother = objMother;
    father = objFather;
  }
}


var cowMOTHER = new Cow(nameMOTHER);

status = inSection(1);
actual = Cow.count;
expect = 1;
addThis();

status = inSection(2);
actual = typeof cowMOTHER;
expect = TYPE_OBJECT;
addThis();

status = inSection(3);
actual = cowMOTHER is Cow;
expect = true;
addThis();

status = inSection(4);
actual = cowMOTHER is Object;
expect = true;
addThis();

status = inSection(5);
actual = cowMOTHER.name;
expect = nameMOTHER;
addThis();

status = inSection(6);
actual = cowMOTHER[propNAME];
expect = nameMOTHER;
addThis();

status = inSection(7);
actual = cowMOTHER[propNONEXIST];
expect = undefined;
addThis();


var cowFATHER = new Cow(nameFATHER);

status = inSection(8);
actual = Cow.count;
expect = 2;
addThis();

status = inSection(9);
actual = typeof cowFATHER;
expect = TYPE_OBJECT;
addThis();

status = inSection(10);
actual = cowFATHER is Cow
expect = true;
addThis();

status = inSection(11);
actual = cowFATHER is Object;
expect = true;
addThis();

status = inSection(12);
actual = cowFATHER.name;
expect = nameFATHER;
addThis();

status = inSection(13);
actual = cowFATHER[propNAME];
expect = nameFATHER;
addThis();

status = inSection(14);
actual = cowFATHER[propNONEXIST];
expect = undefined;
addThis();


var cowCHILD = new Calf(nameCHILD, cowMOTHER, cowFATHER);

status = inSection(15);
actual = Cow.count;
expect = 3;
addThis();

status = inSection(16);
actual = Calf.count;
expect = Cow.count;
addThis();

status = inSection(17);
actual = typeof cowCHILD;
expect = TYPE_OBJECT;
addThis();

status = inSection(18);
actual = cowCHILD is Calf;
expect = true;
addThis();

status = inSection(19);
actual = cowCHILD is Cow;
expect = true;
addThis();

status = inSection(20);
actual = cowCHILD is Object;
expect = true;
addThis();

status = inSection(21);
actual = cowCHILD.mother;
expect = cowMOTHER;
addThis();

status = inSection(22);
actual = cowCHILD[propMOTHER];
expect = cowMOTHER;
addThis();

status = inSection(23);
actual = cowCHILD.mother.name;
expect = nameMOTHER;
addThis();

status = inSection(24);
actual = cowCHILD.mother[propNAME];
expect = nameMOTHER;
addThis();

status = inSection(25);
actual = cowCHILD[propMOTHER].name;
expect = nameMOTHER;
addThis();

status = inSection(26);
actual = cowCHILD[propMOTHER][propNAME];
expect = nameMOTHER;
addThis();

status = inSection(27);
actual = cowCHILD.father;
expect = cowFATHER;
addThis();

status = inSection(28);
actual = cowCHILD[propFATHER];
expect = cowFATHER;
addThis();

status = inSection(29);
actual = cowCHILD.father.name;
expect = nameFATHER;
addThis();

status = inSection(30);
actual = cowCHILD.father[propNAME];
expect = nameFATHER;
addThis();

status = inSection(31);
actual = cowCHILD[propFATHER].name;
expect = nameFATHER;
addThis();

status = inSection(32);
actual = cowCHILD[propFATHER][propNAME];
expect = nameFATHER;
addThis();

status = inSection(33);
actual = cowCHILD.name;
expect = nameCHILD;
addThis();

status = inSection(34);
actual = cowCHILD[propNAME];
expect = nameCHILD;
addThis();

status = inSection(35);
actual = cowCHILD[propNONEXIST];
expect = undefined;
addThis();


cowCHILD.name = nameCHILD;

status = inSection(36);
actual = cowCHILD.name;
expect = nameCHILD;
addThis();

status = inSection(37);
actual = cowCHILD[propNAME];
expect = nameCHILD;
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
