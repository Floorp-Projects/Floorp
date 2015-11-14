var test = `

class foo extends Array { }

function testArrs(arrs) {
    for (let arr of arrs) {
        assertEq(Object.getPrototypeOf(arr), foo.prototype);
    }
}

var arrs = [];
for (var i = 0; i < 25; i++)
    arrs.push(new foo(1));

testArrs(arrs);

arrs[0].nonIndexedProp = "uhoh";

arrs.push(new foo(1));

testArrs(arrs);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
