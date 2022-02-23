
var intrinsic_names = [
    "IsConstructor",    // Implementation in C++
    "ArrayMap",         // Implementation in JS
    "localeCache",      // Self-hosting variable
];

for (var name of intrinsic_names) {
    // GetIntrinsic in same global should have consistent values
    assertEq(getSelfHostedValue(name), getSelfHostedValue(name));

    // Different globals shouldn't reuse intrinsics.
    for (var newCompartment of [true, false]) {
        let g = newGlobal({newCompartment});
        let a = evaluate(`getSelfHostedValue("${name}")`, { global: g })
        let b = getSelfHostedValue(name);
        assertEq(a === b, false);
    }
}
