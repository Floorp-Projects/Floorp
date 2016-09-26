;; Functions

(module $M
  (func (export "call") (result i32) (call $g))
  (func $g (result i32) (i32.const 2))
)
(register "M" $M)

(module $N
  (func $f (import "M" "call") (result i32))
  (export "M.call" (func $f))
  (func (export "call M.call") (result i32) (call $f))
  (func (export "call") (result i32) (call $g))
  (func $g (result i32) (i32.const 3))
)

(assert_return (invoke $M "call") (i32.const 2))
(assert_return (invoke $N "M.call") (i32.const 2))
(assert_return (invoke $N "call") (i32.const 3))
(assert_return (invoke $N "call M.call") (i32.const 2))


;; Globals

(module $M
  (global $glob (export "glob") i32 (i32.const 42))
  (func (export "get") (result i32) (get_global $glob))
)
(register "M" $M)

(module $N
  (global $x (import "M" "glob") i32)
  (func $f (import "M" "get") (result i32))
  (export "M.glob" (global $x))
  (export "M.get" (func $f))
  (global $glob (export "glob") i32 (i32.const 43))
  (func (export "get") (result i32) (get_global $glob))
)

(assert_return (get $M "glob") (i32.const 42))
(assert_return (get $N "M.glob") (i32.const 42))
(assert_return (get $N "glob") (i32.const 43))
(assert_return (invoke $M "get") (i32.const 42))
(assert_return (invoke $N "M.get") (i32.const 42))
(assert_return (invoke $N "get") (i32.const 43))


;; Tables

(module $M
  (type (func (result i32)))
  (type (func))

  (table (export "tab") 10 anyfunc)
  (elem (i32.const 2) $g $g $g $g)
  (func $g (result i32) (i32.const 4))
  (func (export "h") (result i32) (i32.const -4))

  (func (export "call") (param i32) (result i32)
    (call_indirect 0 (get_local 0))
  )
)
(register "M" $M)

(module $N
  (type (func))
  (type (func (result i32)))

  (func $f (import "M" "call") (param i32) (result i32))
  (func $h (import "M" "h") (result i32))

  (table anyfunc (elem $g $g $g $h $f))
  (func $g (result i32) (i32.const 5))

  (export "M.call" (func $f))
  (func (export "call M.call") (param i32) (result i32)
    (call $f (get_local 0))
  )
  (func (export "call") (param i32) (result i32)
    (call_indirect 1 (get_local 0))
  )
)

(assert_return (invoke $M "call" (i32.const 2)) (i32.const 4))
(assert_return (invoke $N "M.call" (i32.const 2)) (i32.const 4))
(assert_return (invoke $N "call" (i32.const 2)) (i32.const 5))
(assert_return (invoke $N "call M.call" (i32.const 2)) (i32.const 4))

(assert_trap (invoke $M "call" (i32.const 1)) "uninitialized")
(assert_trap (invoke $N "M.call" (i32.const 1)) "uninitialized")
(assert_return (invoke $N "call" (i32.const 1)) (i32.const 5))
(assert_trap (invoke $N "call M.call" (i32.const 1)) "uninitialized")

(assert_trap (invoke $M "call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $N "M.call" (i32.const 0)) "uninitialized")
(assert_return (invoke $N "call" (i32.const 0)) (i32.const 5))
(assert_trap (invoke $N "call M.call" (i32.const 0)) "uninitialized")

(assert_trap (invoke $M "call" (i32.const 20)) "undefined")
(assert_trap (invoke $N "M.call" (i32.const 20)) "undefined")
(assert_trap (invoke $N "call" (i32.const 7)) "undefined")
(assert_trap (invoke $N "call M.call" (i32.const 20)) "undefined")

(assert_return (invoke $N "call" (i32.const 3)) (i32.const -4))
(assert_trap (invoke $N "call" (i32.const 4)) "indirect call")

(module $O
  (type (func (result i32)))

  (func $h (import "M" "h") (result i32))
  (table (import "M" "tab") 5 anyfunc)
  (elem (i32.const 1) $i $h)
  (func $i (result i32) (i32.const 6))

  (func (export "call") (param i32) (result i32)
    (call_indirect 0 (get_local 0))
  )
)

(assert_return (invoke $M "call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $N "M.call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $N "call M.call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $O "call" (i32.const 3)) (i32.const 4))

(assert_return (invoke $M "call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $N "M.call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $N "call" (i32.const 2)) (i32.const 5))
(assert_return (invoke $N "call M.call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $O "call" (i32.const 2)) (i32.const -4))

(assert_return (invoke $M "call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $N "M.call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $N "call" (i32.const 1)) (i32.const 5))
(assert_return (invoke $N "call M.call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $O "call" (i32.const 1)) (i32.const 6))

(assert_trap (invoke $M "call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $N "M.call" (i32.const 0)) "uninitialized")
(assert_return (invoke $N "call" (i32.const 0)) (i32.const 5))
(assert_trap (invoke $N "call M.call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $O "call" (i32.const 0)) "uninitialized")

(assert_trap (invoke $O "call" (i32.const 20)) "undefined")

(assert_unlinkable
  (module $Q
    (func $host (import "spectest" "print"))
    (table (import "M" "tab") 10 anyfunc)
    (elem (i32.const 7) $own)
    (elem (i32.const 9) $host)
    (func $own (result i32) (i32.const 666))
  )
  "invalid use of host function"
)
(assert_trap (invoke $M "call" (i32.const 7)) "uninitialized")


;; Memories

(module $M
  (memory (export "mem") 1 5)
  (data (i32.const 10) "\00\01\02\03\04\05\06\07\08\09")

  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (get_local 0))
  )
)
(register "M" $M)

(module $N
  (func $loadM (import "M" "load") (param i32) (result i32))

  (memory 1)
  (data (i32.const 10) "\f0\f1\f2\f3\f4\f5")

  (export "M.load" (func $loadM))
  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (get_local 0))
  )
)

(assert_return (invoke $M "load" (i32.const 12)) (i32.const 2))
(assert_return (invoke $N "M.load" (i32.const 12)) (i32.const 2))
(assert_return (invoke $N "load" (i32.const 12)) (i32.const 0xf2))

(module $O
  (memory (import "M" "mem") 1)
  (data (i32.const 5) "\a0\a1\a2\a3\a4\a5\a6\a7")

  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (get_local 0))
  )
)

(assert_return (invoke $M "load" (i32.const 12)) (i32.const 0xa7))
(assert_return (invoke $N "M.load" (i32.const 12)) (i32.const 0xa7))
(assert_return (invoke $N "load" (i32.const 12)) (i32.const 0xf2))
(assert_return (invoke $O "load" (i32.const 12)) (i32.const 0xa7))

(module $P
  (memory (import "M" "mem") 1 8)

  (func (export "grow") (param $a i32) (result i32)
    (grow_memory (get_local 0))
  )
)

(assert_return (invoke $P "grow" (i32.const 0)) (i32.const 1))
(assert_return (invoke $P "grow" (i32.const 2)) (i32.const 1))
(assert_return (invoke $P "grow" (i32.const 0)) (i32.const 3))
(assert_return (invoke $P "grow" (i32.const 1)) (i32.const 3))
(assert_return (invoke $P "grow" (i32.const 1)) (i32.const 4))
(assert_return (invoke $P "grow" (i32.const 0)) (i32.const 5))
(assert_return (invoke $P "grow" (i32.const 1)) (i32.const -1))
(assert_return (invoke $P "grow" (i32.const 0)) (i32.const 5))

(assert_unlinkable
  (module $Q
    (func $host (import "spectest" "print"))
    (memory (import "M" "mem") 1)
    (table 10 anyfunc)
    (data (i32.const 0) "abc")
    (elem (i32.const 9) $host)
    (func $own (result i32) (i32.const 666))
  )
  "invalid use of host function"
)
(assert_return (invoke $M "load" (i32.const 0)) (i32.const 0))
