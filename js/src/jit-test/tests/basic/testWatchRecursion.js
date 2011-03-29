// Test that the watch handler is not called recursively for the same object
// and property.
(function() {
    var obj1 = {}, obj2 = {};
    var handler_entry_count = 0;
    var handler_exit_count = 0;

    obj1.watch('x', handler);
    obj1.watch('y', handler);
    obj2.watch('x', handler);
    obj1.x = 1;
    assertEq(handler_entry_count, 3);
    assertEq(handler_exit_count, 3);

    function handler(id) {
        handler_entry_count++;
        assertEq(handler_exit_count, 0);
        switch (true) {
          case this === obj1 && id === "x":
            assertEq(handler_entry_count, 1);
            obj2.x = 3;
            assertEq(handler_exit_count, 2);
            break;
          case this === obj2 && id === "x":
            assertEq(handler_entry_count, 2);
            obj1.y = 4;
            assertEq(handler_exit_count, 1);
            break;
          default:
            assertEq(this, obj1);
            assertEq(id, "y");
            assertEq(handler_entry_count, 3);

            // We expect no more watch handler invocations
            obj1.x = 5;
            obj1.y = 6;
            obj2.x = 7;
            assertEq(handler_exit_count, 0);
            break;
        }
        ++handler_exit_count;
        assertEq(handler_entry_count, 3);
    }
})();


// Test that run-away recursion in watch handlers is properly handled.  
(function() {
    var obj = {};
    var i = 0;
    try {
        handler();
        throw new Error("Unreachable");
    } catch(e) {
        assertEq(e instanceof InternalError, true);
    }
    
    function handler() {
        var prop = "a" + ++i;
        obj.watch(prop, handler);
        obj[prop] = 2;
    }
})();
