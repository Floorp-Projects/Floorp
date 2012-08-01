// |reftest| pref(javascript.options.xml.content,true) fails
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "11.1.4 - XML Initializer - {} Expressions - 08";

var BUGNUMBER = 325750;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);
printStatus('E4X: inconsistencies in the use of {} syntax Part Deux');

// https://bugzilla.mozilla.org/show_bug.cgi?id=318922
// https://bugzilla.mozilla.org/show_bug.cgi?id=321549

var exprs = [];
var iexpr;

exprs.push({expr: 'b=\'\\\'\';\na=<a>\n  <b c=\'c{b}>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'\\\'\';\na=<a>\n  <b c={b}c\'>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:b{b}>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:b{b}>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:{b}b>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:{b}b>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <{b}b>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <b{b}>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <{b+\'b\'}>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <{b}b>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c=\'c\'{b}>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c={b}>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c={b}\'c\'>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c{b}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b+\'c\'}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b}c=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'm=1;\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=1;\na=new XML(m);', valid: true});
exprs.push({expr: 'm=<m><n>o</n></m>;\na=<a>\n  <b>x {m} z</b>\n</a>;', valid: true});
exprs.push({expr: 'm=<m><n>o</n></m>;\na=<a>\n  <{m}>x  z</{m}>\n</a>;', valid: false});
exprs.push({expr: 'm=<m>o</m>;\na=<a>\n  <{m}>x  z</{m}>\n</a>;', valid: true});
exprs.push({expr: 'm=[1,\'x\'];\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=[1,\'x\'];\na=new XML(m);', valid: false});
exprs.push({expr: 'm=[1];\na=new XML(m);', valid: false});
exprs.push({expr: 'm=\'<m><n>o</n></m>\';\na=<a>\n  <b>x {m} z</b>\n</a>;', valid: true});
exprs.push({expr: 'm=\'<m><n>o</n></m>\';\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=\'x\';\na=new XML(m);', valid: true});
exprs.push({expr: 'm=new Date();\na=new XML(m);', valid: false});
exprs.push({expr: 'm=new Number(\'1\');\na=new XML(m);', valid: true});
exprs.push({expr: 'm=new String(\'x\');\na=new XML(\'<a>\\n  {m}\\n</a>\');', valid: true});
exprs.push({expr: 'm=new String(\'x\');\na=new XML(m);', valid: true});
exprs.push({expr: 'm={a:1,b:\'x\'};\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm={a:1,b:\'x\'};\na=new XML(m);', valid: false});
exprs.push({expr: 'p="p";\nu=\'http://a.uri/\';\na=<a xmlns:p{p}={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'p="p";\nu=\'http://a.uri/\';\na=<a xmlns:{p}p={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'p="pp";\nu=\'http://a.uri/\';\na=<a xmlns:{p}={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={(function(){return u})()}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={\'x\',1,u}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={u}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={var d=2,u}>\n  <p:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:p={n}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:p={u}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:{n}={n}>\n  <p:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:{n}={n}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={n}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={n}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:p={u}>\n  <{p}:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:pp={u}>\n  <p{p}:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nns="ns";\na=<a xml{ns}:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nns="xmlns";\na=<a {ns}:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nxml="xml";\na=<a {xml}ns:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'pp\';\na=<a xmlns:pp={u}>\n  <{p}:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nu2=\'http://uri2.sameprefix/\';\nn=new Namespace(\'p\',u2);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false}); // This should always fail

for (iexpr = 0; iexpr < exprs.length; ++iexpr)
{
  evalStr(exprs, iexpr);
}

END();

function evalStr(exprs, iexpr)
{
  var value;
  var valid;
  var passfail;
  var obj = exprs[iexpr];

  try
  {
    value = eval(obj.expr).toXMLString();
    valid = true;
  }
  catch(ex)
  {
    value = ex + '';
    valid = false;
  }

  passfail = (valid === obj.valid);

  msg = iexpr + ': ' + (passfail ? 'PASS':'FAIL') +
        ' expected: ' + (obj.valid ? 'valid':'invalid') +
        ', actual: ' + (valid ? 'valid':'invalid') + '\n' +
        'input: ' + '\n' +
        obj.expr + '\n' +
        'output: ' + '\n' +
        value + '\n\n';
 
  printStatus(msg);

  TEST(iexpr, obj.valid, valid);

  return passfail;
}


