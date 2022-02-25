// |jit-test| skip-if: !wasmCachingEnabled()

load(libdir + "wasm-caching.js");

// Test that the tag section is cached correctly
testCached(`(module
	(tag $t (export "t"))
	(func (export "r")
		throw $t
	)
)`, {}, i => {
	assertErrorMessage(() => i.exports.r(), WebAssembly.Exception, /.*/);
});

// Test that try notes are cached correctly
testCached(`(module
	(tag $t)
	(func (export "r") (result i32)
		try (result i32)
			throw $t
			i32.const 0
		catch $t
			i32.const 1
		end
	)
)`, {}, i => {
	assertEq(i.exports.r(), 1, "caught");
});
