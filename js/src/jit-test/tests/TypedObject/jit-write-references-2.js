// |jit-test| skip-if: typeof Intl === 'undefined'
try {
    gczeal(4)
} catch (exc) {}
var T = TypedObject;
var ValueStruct = new T.StructType({
    f: T.Any
})
var v = new ValueStruct;
new class get extends Number {};
function writeValue(o, v) {
  return o.f = v;
}
for (var i = 0; i < 5; i++)
  writeValue(v, {}, "helo")
