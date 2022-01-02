(function() {
    var o = {'arguments': 42};
    with (o) { // Definitely forces heavyweight.
        // Note syntax is not a property access.
        reportCompare(delete arguments, true,
                      "arguments property deletion within with block");
    }
    reportCompare('arguments' in o, false,
                  "property deletion observable");
})();

(function() {
    var o = {'arguments': 42};
    delete o.arguments;
    reportCompare('arguments' in o, false,
                  "arguments property deletion with property access syntax");
})();

(function() {
    var arguments = 42; // Forces heavyweight.
    reportCompare(delete arguments, false,
                  "arguments variable");
})();

(function() {
    reportCompare(delete arguments, false, "arguments object");
})();
