let bytes = wasmTextToBinary(`(module
  (type (;0;) (func))
  (func (;0;) (type 0)
    block  ;; label = @1
      try  ;; label = @2
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
        call 0
      delegate 0
    end)
  (export "" (func 0)))`);
let module = new WebAssembly.Module(bytes);
