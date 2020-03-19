// Point-testing various optimizations in the wasm baseline compiler.

// Boolean optimization for control (bug 1286816).
//
// These optimizations combine a test (a comparison or Eqz) with an
// immediately following conditional branch (BrIf, If, and Select), to
// avoid generating a boolean value that is then tested with a
// compare-to-zero.
//
// On AngryBots as of November 2016, 84% of all test instructions
// (measured statically) are optimized by this method.

function testEqzBrIf(value, type, untaken, taken, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (local i32)
			    (local.set 0 (${type}.const ${value}))
			    (local.set 1 (i32.const ${taken}))
			    (block $b
			     (br_if $b (${type}.eqz (local.get 0)))
			     (local.set 1 (i32.const ${untaken})))
			    (local.get 1))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64"].forEach(t => testEqzBrIf(0, t, 37, 42, 42)); // Taken
["i32", "i64"].forEach(t => testEqzBrIf(1, t, 37, 42, 37)); // Untaken

function testCmpBrIf(value, type, untaken, taken, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (local i32)
			    (local.set 1 (i32.const ${taken}))
			    (block $b
			     (br_if $b (${type}.eq (local.get 0) (${type}.const ${value})))
			     (local.set 1 (i32.const ${untaken})))
			    (local.get 1))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64", "f32", "f64"].forEach(t => testCmpBrIf(0, t, 37, 42, 42)); // Branch taken
["i32", "i64", "f32", "f64"].forEach(t => testCmpBrIf(1, t, 37, 42, 37)); // Branch untaken

function testEqzSelect(value, type, iftrue, iffalse, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (local.set 0 (${type}.const ${value}))
			    (select (i32.const ${iftrue})
			            (i32.const ${iffalse})
			            (${type}.eqz (local.get 0))))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64"].forEach(t => testEqzSelect(0, t, 42, 37, 42)); // Select first
["i32", "i64"].forEach(t => testEqzSelect(1, t, 42, 37, 37)); // Select second

function testCmpSelect(value, type, iftrue, iffalse, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (select (i32.const ${iftrue})
			            (i32.const ${iffalse})
			            (${type}.eq (local.get 0) (${type}.const ${value}))))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64", "f32", "f64"].forEach(t => testCmpSelect(0, t, 42, 37, 42)); // Select first
["i32", "i64", "f32", "f64"].forEach(t => testCmpSelect(1, t, 42, 37, 37)); // Select second

function testEqzIf(value, type, trueBranch, falseBranch, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (local i32)
			    (local.set 0 (${type}.const ${value}))
			    (if (${type}.eqz (local.get 0))
				(local.set 1 (i32.const ${trueBranch}))
			        (local.set 1 (i32.const ${falseBranch})))
			    (local.get 1))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64"].forEach(t => testEqzIf(0, t, 42, 37, 42)); // Taken
["i32", "i64"].forEach(t => testEqzIf(1, t, 42, 37, 37)); // Untaken

function testCmpIf(value, type, trueBranch, falseBranch, expected) {
    var f = wasmEvalText(`(module
			   (func (result i32)
			    (local ${type})
			    (local i32)
			    (if (${type}.eq (local.get 0) (${type}.const ${value}))
				(local.set 1 (i32.const ${trueBranch}))
			        (local.set 1 (i32.const ${falseBranch})))
			    (local.get 1))
			   (export "f" 0))`).exports["f"];
    assertEq(f(), expected);
}

["i32", "i64", "f32", "f64"].forEach(t => testCmpIf(0, t, 42, 37, 42)); // Taken
["i32", "i64", "f32", "f64"].forEach(t => testCmpIf(1, t, 42, 37, 37)); // Untaken
