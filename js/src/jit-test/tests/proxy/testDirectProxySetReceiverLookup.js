// Assigning to a proxy with no set handler causes the proxy's
// getOwnPropertyDescriptor handler to be called just before defineProperty
// in some cases. (ES6 draft rev 28, 2014 Oct 14, 9.1.9 step 5.c.)

var attrs = ["configurable", "enumerable", "writable", "value", "get", "set"];

function test(target, id, existingDesc, expectedDesc) {
    var log = "";
    var p = new Proxy(target, {
        getOwnPropertyDescriptor(t, idarg) {
            assertEq(idarg, id);
            log += "g";
            return existingDesc;
        },
        defineProperty(t, idarg, desc) {
            assertEq(idarg, id);
            for (var attr of attrs) {
                var args = uneval([target, id, existingDesc]).slice(1, -1);
                assertEq(attr in desc, attr in expectedDesc,
                         `test(${args}), checking existence of desc.${attr}`);
                assertEq(desc[attr], expectedDesc[attr],
                         `test(${args}), checking value of desc.${attr}`);
            }
            log += "d";
            return true;
        }
    });
    p[id] = "pizza";
    assertEq(log, "gd");
}

var fullDesc = {
    configurable: true,
    enumerable: true,
    writable: true,
    value: "pizza"
};
var valueOnlyDesc = {
    value: "pizza"
};
var sealedDesc = {
    configurable: false,
    enumerable: true,
    writable: true,
    value: "pizza"
};

test({}, "x", undefined, fullDesc);
test({}, "x", fullDesc, valueOnlyDesc);
test({x: 1}, "x", undefined, fullDesc);
test({x: 1}, "x", fullDesc, valueOnlyDesc);
test(Object.seal({x: 1}), "x", sealedDesc, valueOnlyDesc);
test(Object.create({x: 1}), "x", undefined, fullDesc);
test([0, 1, 2], "2", undefined, fullDesc);
test([0, 1, 2], "2", fullDesc, valueOnlyDesc);
test([0, 1, 2], "3", undefined, fullDesc);
test([0, 1, 2], "3", fullDesc, valueOnlyDesc);
