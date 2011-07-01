// don't crash

var regexp1 = /(?:(?=g))|(?:m).{2147483648,}/;
var regexp2 = /(?:(?=g)).{2147483648,}/;

