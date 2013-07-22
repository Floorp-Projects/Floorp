// |jit-test| error: Error;

var len = 2;
function add1(x) { return x+1; }
var p = new ParallelArray(len, add1);
var idx = [0,0].concat(build(len-4, add1)).concat([len-3,len-3]);
var revidx = idx.reverse();
var r = p.scatter(revidx, 0, function (x,y) { return x+y; }, len-2, {});
