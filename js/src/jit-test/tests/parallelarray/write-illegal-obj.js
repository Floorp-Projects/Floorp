load(libdir + "parallelarray-helpers.js");

function buildSimple() {

    let obj = { x: 1, y: 2, z: 3 };

    new ParallelArray(minItemsTestingThreshold, function(i) {
        obj.x += 1;
        obj.y += 1;
        obj.z += 1;
        return obj;
    }, {mode: "par", expect: "disqualified"});

    print(obj.x);
    print(obj.y);
    print(obj.z);

    // new ParallelArray(256, function(i) {
    //     var o;
    //     if ((i % 2) == 0) {
    //         o = obj;
    //     } else {
    //         o = {x: 1, y: 2, z: 3};
    //     }
    //     o.x += 1;
    //     o.y += 1;
    //     o.z += 1;
    //     return o;
    // }, {mode: "par", expect: "bail"});

}

buildSimple();
