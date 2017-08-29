// This was the actual bug
assertRaises(StopIteration, function() {
    Iterator.prototype.next();
    Iterator.prototype.next();
});

// The error should have triggered here, but was masked by a latent bug
assertRaises(StopIteration, function() {
    Iterator.prototype.next();
});

// Found by fuzzing
assertRaises(StopIteration, function() {
    (new Iterator({})).__proto__.next();
});


function assertRaises(exc, callback) {
    var caught = false;
    try {
        callback();
    } catch (e) {
        assertEq(e instanceof StopIteration, true);
        caught = true;
    }
    assertEq(caught, true);
}
