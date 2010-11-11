// Test using 'while' with 'continue' -- the most ancient, the most powerful,
// the most rare of all coding incantations.

var i = 0;
while (i < HOTLOOP+4) {
    ++i;
    continue;
}
assertEq(i, HOTLOOP+4);

