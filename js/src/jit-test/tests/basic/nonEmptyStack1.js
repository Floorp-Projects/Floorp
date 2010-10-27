function nonEmptyStack1Helper(o, farble) {
    var a = [];
    var j = 0;
    for (var i in o)
        a[j++] = i;
    return a.join("");
}

function nonEmptyStack1() {
    return nonEmptyStack1Helper({a:1,b:2,c:3,d:4,e:5,f:6,g:7,h:8}, "hi");
}

assertEq(nonEmptyStack1(), "abcdefgh");
