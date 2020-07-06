for (let i = 0; i < 2; ++i) {
    // Alternate between even and odd values.
    let power = 1000 + i;

    // Math.pow(negative-int, large-negative-value) is +0 for even and -0 for odd values.
    let expected = (power & 1) === 0 ? +0 : -0;

    assertEq(Math.pow(-3, -power), expected);
}
