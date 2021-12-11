/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 433672;
var summary = 'operator evaluation order';
var actual = '';
var expect = '';

function makeObject(label) 
{
  var o = (function (){});

  o.label    = label;
  o.valueOf  = (function() { actual += this.label + ' valueOf, ';  return Object.prototype.valueOf.call(this); });
  o.toString = (function() { actual += this.label + ' toString, '; return Object.prototype.toString.call(this); });

  return o;
}

operators = [
  {section: '11.5.1', operator: '*'},
  {section: '11.5.2', operator: '/'},
  {section: '11.5.3', operator: '%'},
  {section: '11.6.1', operator: '+'},
  {section: '11.6.2', operator: '-'},
  {section: '11.7.1', operator: '<<'},
  {section: '11.7.2', operator: '>>'},
  {section: '11.7.3', operator: '>>>'},
  {section: '11.8.1', operator: '<'},
  {section: '11.8.2', operator: '>'},
  {section: '11.8.3', operator: '<='},
  {section: '11.8.4', operator: '>='},
  {section: '11.10', operator: '&'},
  {section: '11.10', operator: '^'},
  {section: '11.10', operator: '|'},
  {section: '11.13.2', operator: '*='},
  {section: '11.13.2', operator: '/='},
  {section: '11.13.2', operator: '%='},
  {section: '11.13.2', operator: '+='},
  {section: '11.13.2', operator: '<<='},
  {section: '11.13.2', operator: '>>='},
  {section: '11.13.2', operator: '>>>='},
  {section: '11.13.2', operator: '&='},
  {section: '11.13.2', operator: '^='},
  {section: '11.13.2', operator: '|='},
  ];

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i = 0; i < operators.length; i++)
  {
    expect = 'left valueOf, left toString, right valueOf, right toString, ';
    actual = '';

    var left  = makeObject('left');
    var right = makeObject('right');

    eval('left ' + operators[i].operator + ' right');

    reportCompare(expect, actual, summary + ': ' + operators[i].section + ' ' + operators[i].operator);
  }
}
