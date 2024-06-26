// Check if complex types with local refs are properly validated.

var ins = wasmEvalText(`(module
  (rec
    (type $t0 (struct))
    (type $t1 (sub (func (result (ref null $t1)))))
  )
)`);

var ins = wasmEvalText(`(module
  (rec
    (type $t0 (struct))
    (type $t1 (sub (func (result (ref null $t0)))))
  )
  (func (type $t1) (result (ref null $t0))
    ref.null $t0
  )
)`);
