if (!this.hasOwnProperty("TypedObject"))
  quit();

gczeal(2);
var Vec3u16Type = TypedObject.uint16.array(3);
function foo_u16(n) {
    if (n == 0)
        return;
    var i = 0;
    var vec = new Vec3u16Type([i, i+1, i+2]);
    var sum = vec[0] + vec[1] + vec[(/[]/g )];
    foo_u16(n - 1);
}
foo_u16(100);
