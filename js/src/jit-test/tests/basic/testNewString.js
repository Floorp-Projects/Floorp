function testNewString()
{
  var o = { toString: function() { return "string"; } };
  var r = [];
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(o));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(3));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(2.5));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String("string"));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(null));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(true));
  for (var i = 0; i < 5; i++)
    r.push(typeof new String(undefined));
  return r.length === 35 && r.every(function(v) { return v === "object"; });
}
assertEq(testNewString(), true);
