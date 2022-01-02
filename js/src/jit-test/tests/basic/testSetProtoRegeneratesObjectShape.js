function testSetProtoRegeneratesObjectShape()
{
  var f = function() {};
  var g = function() {};
  g.prototype.__proto__ = {};

  function iq(obj)
  {
    for (var i = 0; i < 10; ++i)
      "" + obj.prototype;
  }

  iq(f);
  iq(f);
  iq(f);
  iq(f);
  iq(g);

  if (shapeOf(f.prototype) === shapeOf(g.prototype))
    return "object shapes same after proto of one is changed";

  return true;
}
assertEq(testSetProtoRegeneratesObjectShape(), true);
