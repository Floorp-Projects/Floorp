// vim: set ts=8 sts=4 et sw=4 tw=99:

var count = 0;
this.watch("x", function() {
    count++;
});
for(var i=0; i<10; i++) {
    x = 2;
}
assertEq(count, 10);
