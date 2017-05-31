// |jit-test| error: ReferenceError

var localstr = "";
for (var i = 0; i < 0xFFFC; ++i)
  localstr += ('\f') + i + "; ";
var arg = "x";
var body = localstr + "for (var i = 0; i < 4; ++i) arr[i](x-1);";
(new Function(arg, body))(1000);
