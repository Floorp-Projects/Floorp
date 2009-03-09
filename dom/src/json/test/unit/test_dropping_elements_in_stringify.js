var w = {foo: 123};
var x = {foo: 123, bar: function () {}};
var y = {foo: 123, bar: function () {}, baz: 123};
var z = {foo: 123, bar: <x><y></y></x>, baz: 123};

function run_test() {
  do_check_eq('{"foo":123}', JSON.stringify(w));
  do_check_eq('{"foo":123}', JSON.stringify(x));
  do_check_eq('{"foo":123,"baz":123}', JSON.stringify(y));
  do_check_eq('{"foo":123,"baz":123}', JSON.stringify(z));
}
