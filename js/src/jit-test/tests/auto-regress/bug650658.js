// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-ac0989a03bf1-linux
// Flags: -m -n -a
//
AddRegExpCases(/a*b/, "xxx", 0, null );
AddRegExpCases(/x\d\dy/, "abcx45ysss235", 3,[] );
function AddRegExpCases(regexp, pattern, index, matches_array )
        (matches_array.length, regexp)
