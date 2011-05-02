var HAVE_TM = 'tracemonkey' in this;
var HOTLOOP = HAVE_TM ? tracemonkey : 8;
with(evalcx(''))(function eval() {}, this.__defineGetter__("x", Function));
var i = 0;
var o;
new(x);
