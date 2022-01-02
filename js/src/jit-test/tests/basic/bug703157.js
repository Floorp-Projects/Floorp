// Define a test object
var test = {x:1,y:2};

// Put the object into dictionary mode by deleting 
// a property that was not the last one added.
delete test.x;

// Define a an accessor property with a setter that 
// itself calls Object.defineProperty
Object.defineProperty(test, "foo", {
    get: function() { return 1; },
    set: function(v) { 
        Object.defineProperty(this, "foo", { value: v });
        // Prints the correct object descriptor
        assertEq(this.foo, 33);
    },
    configurable: true
});

// Add another property, so generateOwnShape does not replace the foo property.
test.other = 0;

// This line prints 1, as expected
assertEq(test.foo, 1);

// Now set the property.  This calls the setter method above.
// And the setter method prints the expected value and property descriptor.
test.foo = 33;

// Finally read the newly set value.
assertEq(test.foo, 33);

// Check that enumeration order is correct.
var arr = [];
for (var x in test) arr.push(x);
assertEq("" + arr, "y,other");
