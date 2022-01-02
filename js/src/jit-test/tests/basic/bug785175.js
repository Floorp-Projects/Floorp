var str = ' ';
// Generate a 4MB string = 2^(20+2)
for (var i = 0; i < 22; i++) {
    str = str + str;
}
str += 'var a = 1 + 1;';

// don't throw an exception even though the column numbers cannot be maintained
eval(str);
