// |jit-test| --arm-asm-nop-fill=1
var f = wasmEvalText(`(module (func (result i32) (param i32)
      (block $0
       (block $1
        (block $2
         (block $default
          (br_table $0 $1 $2 $default (local.get 0))))))
      (return (i32.const 0)))
    (export "" 0)
)`).exports[""];

f(0);
