// Binary: cache/js-dbg-64-1fd6c40d3852-linux
// Flags: --ion-eager
//

var cnName = 'name';
var cnNameGetter = function() {this.nameGETS++; return this._name;};
obj = (new (function  (  )  {  }  )         );
obj.__defineGetter__(cnName, cnNameGetter);
function lameFunc(x, y) {
  var lsw = (x & 0xFFFF) + (y & 0xFFFF);
  var msw = (obj.name) + (y >> 16) + (lsw >> 16);
}
function runSomeTimes(func, iters) {
    for (var i = 0; i < iters; ++i) {
        result = func(42, 42);
    }
}
for (var i = 0; i < 11000; ++i)
    runSomeTimes(lameFunc, 1);
