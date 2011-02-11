// vim: set ts=4 sw=4 tw=99 et:

var o1 = {p1: 1};
var o2 = {p1: 1, p2: 2};

for(var x in o1) {
    for(var y in o2) {
        delete o2.p2;
    }
}

/* Don't fail cx->enumerators == obj assert, see bug comment #31 */

