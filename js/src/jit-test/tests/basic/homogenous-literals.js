
function processNoProperty(a) {
    var total = 0;
    for (var i = 0; i < a.length; i++) {
        var sa = a[i];
        for (var j = 0; j < sa.length; j++)
            total += sa[j];
    }
    assertEq(total, 22);
}

var literalArray = [
    [1,2,3,4],
    [1.5,2.5,3.5,4.5]
];

var jsonArray = JSON.parse(`[
    [1,2,3,4],
    [1.5,2.5,3.5,4.5]
]`);

for (var i = 0; i < 1000; i++) {
    processNoProperty(literalArray);
    processNoProperty(jsonArray);
}

function processWithProperty(a) {
    var total = 0;
    for (var i = 0; i < a.length; i++) {
        var sa = a[i].p;
        for (var j = 0; j < sa.length; j++)
            total += sa[j];
    }
    assertEq(total, 22);
}

var literalPropertyArray = [
    {p:[1,2,3,4]},
    {p:[1.5,2.5,3.5,4.5]}
];

var jsonPropertyArray = JSON.parse(`[
    {"p":[1,2,3,4]},
    {"p":[1.5,2.5,3.5,4.5]}
]`);

for (var i = 0; i < 1000; i++) {
    processWithProperty(literalPropertyArray);
    processWithProperty(jsonPropertyArray);
}
