// Test using 'while' with 'continue' -- the most ancient, the most powerful,
// the most rare of all coding incantations.

var i = 0;
while (i < 12) {
    ++i;
    continue;
}
assertEq(i, 12);

