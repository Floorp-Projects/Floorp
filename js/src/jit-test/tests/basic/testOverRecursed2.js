// |jit-test| error:InternalError

var a = [];
var sort = a.sort.bind(a);
a.push(sort);
a.push(sort);
sort(sort);
