// Binary: cache/js-opt-32-2dc40eb83023-linux
// Flags: -m -n -a
//
function toPrinted(value)
  value = value.replace(/\\n/g, 'NL')
               .replace(/\\r/g, 'CR')
               .replace(/[^\x20-\x7E]+/g, escapeString);
function escapeString (str)
{
  var a, b, c, d;
  var len = str.length;
  var result = "";
  var digits = ["0", "1", "2", "3", "4", "5", "6", "7",
                "8", "9", "A", "B", "C", "D", "E", "F"];
  for (var i=0; i<len; i++)
  {
    var ch = str.charCodeAt(i);
    a = digits[ch & 0xf];
    if (ch)
    {
      c = digits[ch & 0xf];
      ch >>= 4;
      d = digits[ch & 0xf];
      result += "\\u" + d + c + b + a;
    }
  }
}
function reportCompare (expected, actual, description) {
function test() {
    try
    {
    }
    catch(e)
    {
    }
  }
}
try {
gczeal(2,4);
function setprop() {
}
} catch(exc1) {}
var trimMethods = ['trim', 'trimLeft', 'trimRight'];
var whitespace = [
  {s : '\u2028', t : 'LINE SEPARATOR'},
  ];
for (var j = 0; j < trimMethods.length; ++j)
{
  var method = trimMethods[j];
  for (var i = 0; i < whitespace.length; ++i)
  {
    var v = whitespace[i].s;
    var t = whitespace[i].t;
    v = v + v + v;
    str      = v;
    expected = '';
    actual   = str[method]();
    reportCompare(expected, actual, t + ':' + '"' + toPrinted(str) + '".' + method + '()');
  }
}
