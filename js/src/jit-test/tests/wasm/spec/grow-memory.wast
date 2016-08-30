;; Withut a memory, can't use current_memory and grow_memory.
(assert_invalid
  (module
    (func $cm (result i32)
      (current_memory)
    )
  )
  "memory operators require a memory section"
)

(assert_invalid
  (module
    (func $gm (param i32) (result i32)
      (grow_memory (get_local 0))
    )
  )
  "memory operators require a memory section"
)

;; Test Current Memory
(module (memory 0 10)
  (func $gm (param i32) (result i32)
    (grow_memory (get_local 0))
  )

  (func $cm (result i32)
    (current_memory)
  )

  (func $ldst8 (param i32) (param i32) (result i32)
    (block
      (i32.store8 (get_local 0) (get_local 1))
      (i32.load8_u (get_local 0))
    )
  )

  (func $ldst16 (param i32) (param i32) (result i32)
    (block
      (i32.store16 (get_local 0) (get_local 1))
      (i32.load16_u (get_local 0))
    )
  )

  (func $ldst32 (param i32) (param i32) (result i32)
    (block
      (i32.store (get_local 0) (get_local 1))
      (i32.load (get_local 0))
    )
  )

  (func $ldst64 (param i32) (param i64) (result i64)
    (block
      (i64.store (get_local 0) (get_local 1))
      (i64.load (get_local 0))
    )
  )

  (export "cm" $cm)
  (export "gm" $gm)
  (export "ldst8" $ldst8)
  (export "ldst16" $ldst16)
  (export "ldst32" $ldst32)
  (export "ldst64" $ldst64)
)

;; Call current_memory on 0-sized memory
(assert_return (invoke "cm") (i32.const 0))

;; Growing by 0 is ok and doesn't map any new pages
(assert_return (invoke "gm" (i32.const 0)) (i32.const 0))
(assert_return (invoke "cm") (i32.const 0))
(assert_trap (invoke "ldst8" (i32.const 0) (i32.const 42)) "out of bounds memory access")

;; Can't grow by more than whats allowed
(assert_return (invoke "gm" (i32.const 11)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 0))
(assert_trap (invoke "ldst8" (i32.const 0) (i32.const 42)) "out of bounds memory access")

;; Growing by X enables exactly X pages
(assert_return (invoke "gm" (i32.const 1)) (i32.const 0))
(assert_return (invoke "cm") (i32.const 1))
(assert_return (invoke "ldst8" (i32.const 0) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst8" (i32.const 65535) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst16" (i32.const 65534) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst32" (i32.const 65532) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst64" (i32.const 65528) (i64.const 42)) (i64.const 42))
(assert_trap (invoke "ldst8" (i32.const 65536) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst16" (i32.const 65535) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst32" (i32.const 65533) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst64" (i32.const 65529) (i64.const 42)) "out of bounds memory access")

;; grow_memory returns last page size and again we've added only as many pages as requested.
(assert_return (invoke "gm" (i32.const 2)) (i32.const 1))
(assert_return (invoke "cm") (i32.const 3))

;; and again we have only allocated 2 additional pages.
(assert_return (invoke "ldst8" (i32.const 0) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst8" (i32.const 196607) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst16" (i32.const 196606) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst32" (i32.const 196604) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst64" (i32.const 196600) (i64.const 42)) (i64.const 42))
(assert_trap (invoke "ldst8" (i32.const 196608) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst16" (i32.const 196607) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst32" (i32.const 196605) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst64" (i32.const 196601) (i64.const 42)) "out of bounds memory access")

;; One more time can't grow by more than whats allowed and failed growing doesn't add new pages
(assert_return (invoke "gm" (i32.const 8)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 3))
(assert_return (invoke "ldst8" (i32.const 196607) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst16" (i32.const 196606) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst32" (i32.const 196604) (i32.const 42)) (i32.const 42))
(assert_return (invoke "ldst64" (i32.const 196600) (i64.const 42)) (i64.const 42))
(assert_trap (invoke "ldst8" (i32.const 196608) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst16" (i32.const 196607) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst32" (i32.const 196605) (i32.const 42)) "out of bounds memory access")
(assert_trap (invoke "ldst64" (i32.const 196601) (i64.const 42)) "out of bounds memory access")

;; Can't grow by number of pages that would overflow UINT32 when scaled by the wasm page size
(assert_return (invoke "gm" (i32.const 65534)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 3))
(assert_return (invoke "gm" (i32.const 65535)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 3))
(assert_return (invoke "gm" (i32.const 65536)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 3))
(assert_return (invoke "gm" (i32.const 65537)) (i32.const -1))
(assert_return (invoke "cm") (i32.const 3))
