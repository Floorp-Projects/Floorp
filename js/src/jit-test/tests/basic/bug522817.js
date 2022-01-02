// This test should not assert in a debug build.

var q1={};
var $native = function () {
    for (var i = 0, l = arguments.length; i < l; i++) {
        arguments[i].extend = function (props) {};
    }
};
$native(q1, Array, String, Number);
Array.extend({});
Number.extend({});
Object.Native = function () {
    for (var i = 0; i < arguments.length; i++) {
      arguments[i].eeeeee = (function(){});
    }
};
new Object.Native(q1, Array, String, Number);
