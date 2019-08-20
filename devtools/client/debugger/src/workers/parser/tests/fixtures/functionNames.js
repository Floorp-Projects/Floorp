/* eslint-disable */

({
  foo: function() {},
  "foo": function() {},
  42: function() {},

  foo() {},
  "foo"() {},
  42() {},
});

foo = function() {};
obj.foo = function() {};

var foo = function(){};
var [foo = function(){}] = [];
var {foo = function(){}} = {};

[foo = function(){}] = [];
({foo = function(){}} = {});
({bar: foo = function(){}} = {});

function fn([foo = function(){}]){}
function f2({foo = function(){}} = {}){}
function f3({bar: foo = function(){}} = {}){}

class Cls {
  foo = function() {};
  "foo" = function() {};
  42 = function() {};

  foo() {}
  "foo"() {}
  42() {}
}

(function(){});

export default function (){}

const defaultObj = {a: 1};
const defaultArr = ['smthng'];
function a(first, second){}
function b(first = 'bla', second){}
function c(first = {}, second){}
function d(first = [], second){}
function e(first = defaultObj, second){}
function f(first = defaultArr, second){}
function g(first = null, second){}
