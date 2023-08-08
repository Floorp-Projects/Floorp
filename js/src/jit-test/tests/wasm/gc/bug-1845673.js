// |jit-test| skip-if: !wasmGcEnabled()

let {f} = wasmEvalText(`(module
	(type (struct))
	(func (export "f") (result anyref)
		ref.null 0
		br_on_cast_fail 0 (ref null 0) (ref null 0)
		ref.null 0
		br_on_cast_fail 0 (ref null 0) (ref 0)
		br_on_cast_fail 0 (ref null 0) (ref null 0)
		unreachable
	)
)`).exports;
f();
