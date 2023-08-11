var g = newGlobal({sameCompartmentAs: this});
g.evaluate(`enableShellAllocationMetadataBuilder()`);

function f() {
    // Ensure a match stub is created for the zone.
    var re = /abc.+/;
    for (var i = 0; i < 100; i++) {
        assertEq(re.exec("..abcd").index, 2);
    }
    // Allocated match result objects in the realm with the metadata hook
    // must have metadata.
    g.evaluate(`
    var re = /abc.+/;
    for (var i = 0; i < 100; i++) {
        var obj = re.exec("..abcd");
        assertEq(getAllocationMetadata(obj).stack.length > 0, true);
    }
    `)
}
f();
