let { exports: { f } } = wasmEvalText(`
(module
 (memory $0 1 1)

 (func (export "f") (result f32)
  (local $0 i32) (local $1 f64) (local $2 i32)

  (local.set 0 (i32.const 134219779))
  (local.set 1 (f64.const 3810600700439633677210579e165))

  (f32.floor
   (loop $label$2 (result f32)
    (br_if $label$2
     (i32.load offset=3 align=2
       (block $label$4 (result i32)
        (drop
         (if (result f64)
             (br_if $label$4
              (i32.const 4883)
                (i32.const -124)
          )
          (then (f64.const 77))
          (else
           (block (result f64)
            (drop
              (br_if $label$4
                (i32.const 4194304)
                (i32.const -8192)
              )
            )
            (return
              (f32.const 4294967296)
            )
           )
          )
         )
        )
          (br_if $label$4
             (br_if $label$4
              (i32.const -90)
               (br_if $label$4
                (br_if $label$4
                 (local.get $2)
                 (i32.const -16)
                )
                (i32.const 15656)
               )
             )
          (block $label$18 (result i32)
           (i32.eqz
            (br_if $label$4
                 (i32.const -1)
                 (i32.const 15)
           )
          )
         )
        )
       )
     )
    )
    (f32.const 23)
   )
  )
 )
)`);

f();
