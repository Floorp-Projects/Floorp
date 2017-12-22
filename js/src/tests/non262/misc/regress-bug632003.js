/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: 
 */
var BUGNUMBER = 632003;
var summary = 'The var statement should add the property to the global if it exists on the prototype';

// Define properties on Object.prototype with various attributes and
// value-getter-setter combinations then check that a var statement
// can always define a variable with the same name in the global object.

if (typeof evaluate != "undefined") {
  var global_case = def_all("global_case");
  evaluate(global_case.source);
  check_values(this, global_case.var_list);
}

var eval_case = def_all("eval_case");
eval(eval_case.source);
check_values(this, eval_case.var_list);

function def_all(prefix)
{
  var builder, index, i, j;

  builder = {source: "", var_list: []};
  index = 0;
  for (i = 0; i <= 1; ++i) {
    for (j = 0; j <= 1; ++j) {
      def({value: index});
      def({value: index, writable: true});
      def({get: Function("return "+index+";")});
      def({set: function() { }});
      def({get: Function("return "+index+";"), set: function() { }});
    }
  }
  return builder;
  
  function def(descriptor_seed)
  {
    var var_name = prefix + index;
    descriptor_seed.configurable = !!i;
    descriptor_seed.enumerable = !!j;
    Object.defineProperty(Object.prototype, var_name, descriptor_seed);
    var var_value = index + 0.5;
    builder.source += "var "+var_name+" = "+var_value+";\n";
    builder.var_list.push({name: var_name, expected_value: var_value});
    ++index;
  }
}

function check_values(obj, var_list)
{
  for (i = 0; i != var_list.length; ++i) {
    var name = var_list[i].name;
    assertEq(obj.hasOwnProperty(name), true);
    assertEq(obj[name], var_list[i].expected_value);
  }
}

reportCompare(true, true);
