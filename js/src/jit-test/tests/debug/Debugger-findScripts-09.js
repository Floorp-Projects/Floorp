// Passing bad query properties to Debugger.prototype.findScripts throws.
load(libdir + 'asserts.js');

var dbg = new Debugger();
var g = newGlobal();
assertEq(dbg.findScripts().length, 0);
assertEq(dbg.findScripts({}).length, 0);

assertEq(dbg.findScripts({global:g}).length, 0);
assertThrowsInstanceOf(function () { dbg.findScripts({global:null}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({global:true}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({global:4}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({global:"I must have fruit!"}); }, TypeError);

assertEq(dbg.findScripts({url:""}).length, 0);
assertThrowsInstanceOf(function () { dbg.findScripts({url:null}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:true}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:4}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:{}}); }, TypeError);

assertEq(dbg.findScripts({url:"", line:1}).length, 0);
assertEq(dbg.findScripts({url:"", line:Math.sqrt(4)}).length, 0);

// A 'line' property without a 'url' property is verboten.
assertThrowsInstanceOf(function () { dbg.findScripts({line:1}); }, TypeError);

assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:null}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:{}}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:true}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:""}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:0}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:-1}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({url:"",line:1.5}); }, TypeError);

// Values of any type for 'innermost' are accepted.
assertEq(dbg.findScripts({url:"", line:1, innermost:true}).length, 0);
assertEq(dbg.findScripts({url:"", line:1, innermost:1}).length, 0);
assertEq(dbg.findScripts({url:"", line:1, innermost:"yes"}).length, 0);
assertEq(dbg.findScripts({url:"", line:1, innermost:{}}).length, 0);
assertEq(dbg.findScripts({url:"", line:1, innermost:[]}).length, 0);

// An 'innermost' property without 'url' and 'line' properties is verboten.
assertThrowsInstanceOf(function () { dbg.findScripts({innermost:true}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({innermost:true, line:1}); }, TypeError);
assertThrowsInstanceOf(function () { dbg.findScripts({innermost:true, url:"foo"}); }, TypeError);
