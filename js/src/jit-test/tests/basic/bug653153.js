// ES5 15.1.2.2 step 1

/*
 * Boundary testing for super-large positive numbers between non-exponential
 * and in-exponential-form.
 *
 * NB: While 1e21 is exactly representable as an IEEE754 double-precision
 * number, its nearest neighboring representable values are a good distance
 * away, 65536 to be precise.
 */

// This is the boundary in theory.
assertEq(parseInt(1e21), 1);

// This is the boundary in practice.
assertEq(parseInt(1e21 - 65537) > 1e20, true);
assertEq(parseInt(1e21 - 65536), 1);
assertEq(parseInt(1e21 + 65536), 1);

// Check that we understand floating point accuracy near the boundary
assertEq(1e21 - 65537 !== 1e21 - 65536, true);
assertEq(1e21 - 65536, 1e21);
assertEq(1e21 + 65535, 1e21);
assertEq(1e21 + 65536, 1e21);

// ES5 leaves exact precision in ToString(bigMagNum) undefined, which
// might make this value inconsistent across implementations (maybe,
// nobody's done the math here).  Regardless, it's definitely a number
// very close to 1, and not a large-magnitude positive number.
assertEq(1e21 + 65537 !== 1e21, true);
assertEq(parseInt(1e21 + 65537) < 1.001, true);


/*
 * Now do the same tests for super-large negative numbers crossing the
 * opposite boundary.
 */

// This is the boundary in theory.
assertEq(parseInt(-1e21), -1);

// This is the boundary in practice.
assertEq(parseInt(-1e21 + 65537) < -1e20, true);
assertEq(parseInt(-1e21 + 65536), -1);
assertEq(parseInt(-1e21 - 65536), -1);

// Check that we understand floating point accuracy near the boundary
assertEq(-1e21 + 65537 !== -1e21 + 65536, true);
assertEq(-1e21 + 65536, -1e21);
assertEq(-1e21 - 65535, -1e21);
assertEq(-1e21 - 65536, -1e21);

// ES5 leaves exact precision in ToString(bigMagNum) undefined, which
// might make this value inconsistent across implementations (maybe,
// nobody's done the math here).  Regardless, it's definitely a number
// very close to -1, and not a large-magnitude negative number.
assertEq(-1e21 - 65537 !== 1e21, true);
assertEq(parseInt(-1e21 - 65537) > -1.001, true);


/* Check values around the boundary. */
arr = [1e0, 5e1, 9e19, 0.1e20, 1.3e20, 1e20, 9e20, 9.99e20, 0.1e21,
       1e21, 1.0e21, 2e21, 2e20, 2.1e22, 9e21, 0.1e22, 1e22, 3e46, 3e23, 3e100, 3.4e200, 7e1000,
       1e21, 1e21+65537, 1e21+65536, 1e21-65536, 1e21-65537]; 

/* Check across a range of values in case we missed anything. */
for (var i = 0; i < 4000; i++) {
    arr.push(1e19 + i*1e19);
}

for (var i in arr) {
    assertEq(parseInt( arr[i]), parseInt(String( arr[i])));
    assertEq(parseInt(-arr[i]), parseInt(String(-arr[i])));
}


