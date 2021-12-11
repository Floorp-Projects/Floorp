// Test that zero-based line numbers supplied by Reflect.parse don't cause
// assertions.

function parseAsModule(source) {
    return Reflect.parse(source, {
        target: "module",
        line: 0
    });
}
parseAsModule("import d from 'a'");
