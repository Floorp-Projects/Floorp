// vim: set ts=4 sw=4 tw=99 et:

var count = 0;
this.watch("x", function() {
    count++;
});
for(var i=0; i<10; i++) {
    x = 2;
}
assertEq(count, 10);
