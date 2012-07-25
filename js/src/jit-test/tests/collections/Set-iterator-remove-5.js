// Removing a Set entry already visited by an iterator does not cause any
// entries to be skipped.

var str = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
var set = Set(str);

var log = '';
var i = 0;
for (var x of set) {
    log += x;
    if (i++ % 5 === 0) {
        // Delete all entries preceding this one.
        for (let y of set) {
            if (y === x)
                break;
            set.delete(y);
        }
    }
}
assertEq(log, str);
assertEq(set.size(), 1);  // Elements 0 to 24 are removed, leaving only 25 (Z).
assertEq(set.has('Z'), true);
