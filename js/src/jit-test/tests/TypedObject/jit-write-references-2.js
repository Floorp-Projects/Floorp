if (typeof TypedObject === "undefined" || typeof Intl === "undefined")
    quit();

try {
    gczeal(4)
} catch (exc) {}
var T = TypedObject;
var ValueStruct = new T.StructType({
    f: T.Any
})
var v = new ValueStruct;
new class get extends Number {};
function writeValue(o, v)
  o.f = v
for (var i = 0; i < 5; i++)
  writeValue(v, {}, "helo")
