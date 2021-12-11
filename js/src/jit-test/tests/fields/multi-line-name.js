// Test that multi-line property names don't trip up source location asserts.

class C {
    'line \
        continuation';

    'line \
        continuation with init' = 1;

    [1 +
        "bar"];

    [2 +
        "baz"] = 2;
}
