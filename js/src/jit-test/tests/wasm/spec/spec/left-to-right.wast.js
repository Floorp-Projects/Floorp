/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ./test/core/left-to-right.wast

// ./test/core/left-to-right.wast:1
let $0 = instantiate(`(module
  (memory 1)

  (type $$i32_T (func (param i32 i32) (result i32)))
  (type $$i64_T (func (param i64 i64) (result i32)))
  (type $$f32_T (func (param f32 f32) (result i32)))
  (type $$f64_T (func (param f64 f64) (result i32)))
  (table funcref
    (elem $$i32_t0 $$i32_t1 $$i64_t0 $$i64_t1 $$f32_t0 $$f32_t1 $$f64_t0 $$f64_t1)
  )

  (func $$i32_t0 (type $$i32_T) (i32.const -1))
  (func $$i32_t1 (type $$i32_T) (i32.const -2))
  (func $$i64_t0 (type $$i64_T) (i32.const -1))
  (func $$i64_t1 (type $$i64_T) (i32.const -2))
  (func $$f32_t0 (type $$f32_T) (i32.const -1))
  (func $$f32_t1 (type $$f32_T) (i32.const -2))
  (func $$f64_t0 (type $$f64_T) (i32.const -1))
  (func $$f64_t1 (type $$f64_T) (i32.const -2))

  ;; The idea is: We reset the memory, then the instruction call $$*_left,
  ;; $$*_right, $$*_another, $$*_callee (for indirect calls), and $$*_bool (when a
  ;; boolean value is needed). These functions all call bump, which shifts the
  ;; memory starting at address 8 up a byte, and then store a unique value at
  ;; address 8. Then we read the 4-byte value at address 8. It should contain
  ;; the correct sequence of unique values if the calls were evaluated in the
  ;; correct order.

  (func $$reset (i32.store (i32.const 8) (i32.const 0)))

  (func $$bump
    (i32.store8 (i32.const 11) (i32.load8_u (i32.const 10)))
    (i32.store8 (i32.const 10) (i32.load8_u (i32.const 9)))
    (i32.store8 (i32.const 9) (i32.load8_u (i32.const 8)))
    (i32.store8 (i32.const 8) (i32.const -3)))

  (func $$get (result i32) (i32.load (i32.const 8)))

  (func $$i32_left (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 1)) (i32.const 0))
  (func $$i32_right (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 2)) (i32.const 1))
  (func $$i32_another (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 3)) (i32.const 1))
  (func $$i32_callee (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 4)) (i32.const 0))
  (func $$i32_bool (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 5)) (i32.const 0))
  (func $$i64_left (result i64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 1)) (i64.const 0))
  (func $$i64_right (result i64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 2)) (i64.const 1))
  (func $$i64_another (result i64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 3)) (i64.const 1))
  (func $$i64_callee (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 4)) (i32.const 2))
  (func $$i64_bool (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 5)) (i32.const 0))
  (func $$f32_left (result f32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 1)) (f32.const 0))
  (func $$f32_right (result f32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 2)) (f32.const 1))
  (func $$f32_another (result f32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 3)) (f32.const 1))
  (func $$f32_callee (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 4)) (i32.const 4))
  (func $$f32_bool (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 5)) (i32.const 0))
  (func $$f64_left (result f64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 1)) (f64.const 0))
  (func $$f64_right (result f64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 2)) (f64.const 1))
  (func $$f64_another (result f64) (call $$bump) (i32.store8 (i32.const 8) (i32.const 3)) (f64.const 1))
  (func $$f64_callee (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 4)) (i32.const 6))
  (func $$f64_bool (result i32) (call $$bump) (i32.store8 (i32.const 8) (i32.const 5)) (i32.const 0))
  (func $$i32_dummy (param i32 i32))
  (func $$i64_dummy (param i64 i64))
  (func $$f32_dummy (param f32 f32))
  (func $$f64_dummy (param f64 f64))

  (func (export "i32_add") (result i32) (call $$reset) (drop (i32.add (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_sub") (result i32) (call $$reset) (drop (i32.sub (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_mul") (result i32) (call $$reset) (drop (i32.mul (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_div_s") (result i32) (call $$reset) (drop (i32.div_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_div_u") (result i32) (call $$reset) (drop (i32.div_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_rem_s") (result i32) (call $$reset) (drop (i32.rem_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_rem_u") (result i32) (call $$reset) (drop (i32.rem_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_and") (result i32) (call $$reset) (drop (i32.and (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_or") (result i32) (call $$reset) (drop (i32.or (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_xor") (result i32) (call $$reset) (drop (i32.xor (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_shl") (result i32) (call $$reset) (drop (i32.shl (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_shr_u") (result i32) (call $$reset) (drop (i32.shr_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_shr_s") (result i32) (call $$reset) (drop (i32.shr_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_eq") (result i32) (call $$reset) (drop (i32.eq (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_ne") (result i32) (call $$reset) (drop (i32.ne (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_lt_s") (result i32) (call $$reset) (drop (i32.lt_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_le_s") (result i32) (call $$reset) (drop (i32.le_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_lt_u") (result i32) (call $$reset) (drop (i32.lt_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_le_u") (result i32) (call $$reset) (drop (i32.le_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_gt_s") (result i32) (call $$reset) (drop (i32.gt_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_ge_s") (result i32) (call $$reset) (drop (i32.ge_s (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_gt_u") (result i32) (call $$reset) (drop (i32.gt_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_ge_u") (result i32) (call $$reset) (drop (i32.ge_u (call $$i32_left) (call $$i32_right))) (call $$get))
  (func (export "i32_store") (result i32) (call $$reset) (i32.store (call $$i32_left) (call $$i32_right)) (call $$get))
  (func (export "i32_store8") (result i32) (call $$reset) (i32.store8 (call $$i32_left) (call $$i32_right)) (call $$get))
  (func (export "i32_store16") (result i32) (call $$reset) (i32.store16 (call $$i32_left) (call $$i32_right)) (call $$get))
  (func (export "i32_call") (result i32) (call $$reset) (call $$i32_dummy (call $$i32_left) (call $$i32_right)) (call $$get))
  (func (export "i32_call_indirect") (result i32) (call $$reset) (drop (call_indirect (type $$i32_T) (call $$i32_left) (call $$i32_right) (call $$i32_callee))) (call $$get))
  (func (export "i32_select") (result i32) (call $$reset) (drop (select (call $$i32_left) (call $$i32_right) (call $$i32_bool))) (call $$get))

  (func (export "i64_add") (result i32) (call $$reset) (drop (i64.add (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_sub") (result i32) (call $$reset) (drop (i64.sub (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_mul") (result i32) (call $$reset) (drop (i64.mul (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_div_s") (result i32) (call $$reset) (drop (i64.div_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_div_u") (result i32) (call $$reset) (drop (i64.div_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_rem_s") (result i32) (call $$reset) (drop (i64.rem_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_rem_u") (result i32) (call $$reset) (drop (i64.rem_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_and") (result i32) (call $$reset) (drop (i64.and (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_or") (result i32) (call $$reset) (drop (i64.or (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_xor") (result i32) (call $$reset) (drop (i64.xor (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_shl") (result i32) (call $$reset) (drop (i64.shl (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_shr_u") (result i32) (call $$reset) (drop (i64.shr_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_shr_s") (result i32) (call $$reset) (drop (i64.shr_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_eq") (result i32) (call $$reset) (drop (i64.eq (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_ne") (result i32) (call $$reset) (drop (i64.ne (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_lt_s") (result i32) (call $$reset) (drop (i64.lt_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_le_s") (result i32) (call $$reset) (drop (i64.le_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_lt_u") (result i32) (call $$reset) (drop (i64.lt_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_le_u") (result i32) (call $$reset) (drop (i64.le_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_gt_s") (result i32) (call $$reset) (drop (i64.gt_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_ge_s") (result i32) (call $$reset) (drop (i64.ge_s (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_gt_u") (result i32) (call $$reset) (drop (i64.gt_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_ge_u") (result i32) (call $$reset) (drop (i64.ge_u (call $$i64_left) (call $$i64_right))) (call $$get))
  (func (export "i64_store") (result i32) (call $$reset) (i64.store (call $$i32_left) (call $$i64_right)) (call $$get))
  (func (export "i64_store8") (result i32) (call $$reset) (i64.store8 (call $$i32_left) (call $$i64_right)) (call $$get))
  (func (export "i64_store16") (result i32) (call $$reset) (i64.store16 (call $$i32_left) (call $$i64_right)) (call $$get))
  (func (export "i64_store32") (result i32) (call $$reset) (i64.store32 (call $$i32_left) (call $$i64_right)) (call $$get))
  (func (export "i64_call") (result i32) (call $$reset) (call $$i64_dummy (call $$i64_left) (call $$i64_right)) (call $$get))
  (func (export "i64_call_indirect") (result i32) (call $$reset) (drop (call_indirect (type $$i64_T) (call $$i64_left) (call $$i64_right) (call $$i64_callee))) (call $$get))
  (func (export "i64_select") (result i32) (call $$reset) (drop (select (call $$i64_left) (call $$i64_right) (call $$i64_bool))) (call $$get))

  (func (export "f32_add") (result i32) (call $$reset) (drop (f32.add (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_sub") (result i32) (call $$reset) (drop (f32.sub (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_mul") (result i32) (call $$reset) (drop (f32.mul (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_div") (result i32) (call $$reset) (drop (f32.div (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_copysign") (result i32) (call $$reset) (drop (f32.copysign (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_eq") (result i32) (call $$reset) (drop (f32.eq (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_ne") (result i32) (call $$reset) (drop (f32.ne (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_lt") (result i32) (call $$reset) (drop (f32.lt (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_le") (result i32) (call $$reset) (drop (f32.le (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_gt") (result i32) (call $$reset) (drop (f32.gt (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_ge") (result i32) (call $$reset) (drop (f32.ge (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_min") (result i32) (call $$reset) (drop (f32.min (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_max") (result i32) (call $$reset) (drop (f32.max (call $$f32_left) (call $$f32_right))) (call $$get))
  (func (export "f32_store") (result i32) (call $$reset) (f32.store (call $$i32_left) (call $$f32_right)) (call $$get))
  (func (export "f32_call") (result i32) (call $$reset) (call $$f32_dummy (call $$f32_left) (call $$f32_right)) (call $$get))
  (func (export "f32_call_indirect") (result i32) (call $$reset) (drop (call_indirect (type $$f32_T) (call $$f32_left) (call $$f32_right) (call $$f32_callee))) (call $$get))
  (func (export "f32_select") (result i32) (call $$reset) (drop (select (call $$f32_left) (call $$f32_right) (call $$f32_bool))) (call $$get))

  (func (export "f64_add") (result i32) (call $$reset) (drop (f64.add (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_sub") (result i32) (call $$reset) (drop (f64.sub (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_mul") (result i32) (call $$reset) (drop (f64.mul (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_div") (result i32) (call $$reset) (drop (f64.div (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_copysign") (result i32) (call $$reset) (drop (f64.copysign (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_eq") (result i32) (call $$reset) (drop (f64.eq (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_ne") (result i32) (call $$reset) (drop (f64.ne (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_lt") (result i32) (call $$reset) (drop (f64.lt (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_le") (result i32) (call $$reset) (drop (f64.le (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_gt") (result i32) (call $$reset) (drop (f64.gt (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_ge") (result i32) (call $$reset) (drop (f64.ge (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_min") (result i32) (call $$reset) (drop (f64.min (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_max") (result i32) (call $$reset) (drop (f64.max (call $$f64_left) (call $$f64_right))) (call $$get))
  (func (export "f64_store") (result i32) (call $$reset) (f64.store (call $$i32_left) (call $$f64_right)) (call $$get))
  (func (export "f64_call") (result i32) (call $$reset) (call $$f64_dummy (call $$f64_left) (call $$f64_right)) (call $$get))
  (func (export "f64_call_indirect") (result i32) (call $$reset) (drop (call_indirect (type $$f64_T) (call $$f64_left) (call $$f64_right) (call $$f64_callee))) (call $$get))
  (func (export "f64_select") (result i32) (call $$reset) (drop (select (call $$f64_left) (call $$f64_right) (call $$f64_bool))) (call $$get))

  (func (export "br_if") (result i32)
    (block (result i32)
      (call $$reset)
      (drop (br_if 0 (call $$i32_left) (i32.and (call $$i32_right) (i32.const 0))))
      (call $$get)
    )
  )
  (func (export "br_table") (result i32)
    (block $$a (result i32)
      (call $$reset)
      (drop
        (block $$b (result i32)
          (br_table $$a $$b (call $$i32_left) (call $$i32_right))
        )
      )
      (call $$get)
    )
  )
)`);

// ./test/core/left-to-right.wast:181
assert_return(() => invoke($0, `i32_add`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:181:59
assert_return(() => invoke($0, `i64_add`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:182
assert_return(() => invoke($0, `i32_sub`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:182:59
assert_return(() => invoke($0, `i64_sub`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:183
assert_return(() => invoke($0, `i32_mul`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:183:59
assert_return(() => invoke($0, `i64_mul`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:184
assert_return(() => invoke($0, `i32_div_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:184:59
assert_return(() => invoke($0, `i64_div_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:185
assert_return(() => invoke($0, `i32_div_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:185:59
assert_return(() => invoke($0, `i64_div_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:186
assert_return(() => invoke($0, `i32_rem_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:186:59
assert_return(() => invoke($0, `i64_rem_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:187
assert_return(() => invoke($0, `i32_rem_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:187:59
assert_return(() => invoke($0, `i64_rem_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:188
assert_return(() => invoke($0, `i32_and`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:188:59
assert_return(() => invoke($0, `i64_and`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:189
assert_return(() => invoke($0, `i32_or`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:189:59
assert_return(() => invoke($0, `i64_or`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:190
assert_return(() => invoke($0, `i32_xor`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:190:59
assert_return(() => invoke($0, `i64_xor`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:191
assert_return(() => invoke($0, `i32_shl`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:191:59
assert_return(() => invoke($0, `i64_shl`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:192
assert_return(() => invoke($0, `i32_shr_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:192:59
assert_return(() => invoke($0, `i64_shr_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:193
assert_return(() => invoke($0, `i32_shr_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:193:59
assert_return(() => invoke($0, `i64_shr_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:194
assert_return(() => invoke($0, `i32_eq`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:194:59
assert_return(() => invoke($0, `i64_eq`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:195
assert_return(() => invoke($0, `i32_ne`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:195:59
assert_return(() => invoke($0, `i64_ne`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:196
assert_return(() => invoke($0, `i32_lt_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:196:59
assert_return(() => invoke($0, `i64_lt_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:197
assert_return(() => invoke($0, `i32_le_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:197:59
assert_return(() => invoke($0, `i64_le_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:198
assert_return(() => invoke($0, `i32_lt_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:198:59
assert_return(() => invoke($0, `i64_lt_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:199
assert_return(() => invoke($0, `i32_le_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:199:59
assert_return(() => invoke($0, `i64_le_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:200
assert_return(() => invoke($0, `i32_gt_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:200:59
assert_return(() => invoke($0, `i64_gt_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:201
assert_return(() => invoke($0, `i32_ge_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:201:59
assert_return(() => invoke($0, `i64_ge_s`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:202
assert_return(() => invoke($0, `i32_gt_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:202:59
assert_return(() => invoke($0, `i64_gt_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:203
assert_return(() => invoke($0, `i32_ge_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:203:59
assert_return(() => invoke($0, `i64_ge_u`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:204
assert_return(() => invoke($0, `i32_store`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:204:59
assert_return(() => invoke($0, `i64_store`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:205
assert_return(() => invoke($0, `i32_store8`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:205:59
assert_return(() => invoke($0, `i64_store8`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:206
assert_return(() => invoke($0, `i32_store16`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:206:59
assert_return(() => invoke($0, `i64_store16`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:207
assert_return(() => invoke($0, `i64_store32`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:208
assert_return(() => invoke($0, `i32_call`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:208:59
assert_return(() => invoke($0, `i64_call`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:209
assert_return(() => invoke($0, `i32_call_indirect`, []), [value("i32", 66052)]);

// ./test/core/left-to-right.wast:210
assert_return(() => invoke($0, `i64_call_indirect`, []), [value("i32", 66052)]);

// ./test/core/left-to-right.wast:211
assert_return(() => invoke($0, `i32_select`, []), [value("i32", 66053)]);

// ./test/core/left-to-right.wast:211:61
assert_return(() => invoke($0, `i64_select`, []), [value("i32", 66053)]);

// ./test/core/left-to-right.wast:213
assert_return(() => invoke($0, `f32_add`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:213:59
assert_return(() => invoke($0, `f64_add`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:214
assert_return(() => invoke($0, `f32_sub`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:214:59
assert_return(() => invoke($0, `f64_sub`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:215
assert_return(() => invoke($0, `f32_mul`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:215:59
assert_return(() => invoke($0, `f64_mul`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:216
assert_return(() => invoke($0, `f32_div`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:216:59
assert_return(() => invoke($0, `f64_div`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:217
assert_return(() => invoke($0, `f32_copysign`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:217:59
assert_return(() => invoke($0, `f64_copysign`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:218
assert_return(() => invoke($0, `f32_eq`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:218:59
assert_return(() => invoke($0, `f64_eq`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:219
assert_return(() => invoke($0, `f32_ne`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:219:59
assert_return(() => invoke($0, `f64_ne`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:220
assert_return(() => invoke($0, `f32_lt`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:220:59
assert_return(() => invoke($0, `f64_lt`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:221
assert_return(() => invoke($0, `f32_le`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:221:59
assert_return(() => invoke($0, `f64_le`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:222
assert_return(() => invoke($0, `f32_gt`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:222:59
assert_return(() => invoke($0, `f64_gt`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:223
assert_return(() => invoke($0, `f32_ge`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:223:59
assert_return(() => invoke($0, `f64_ge`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:224
assert_return(() => invoke($0, `f32_min`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:224:59
assert_return(() => invoke($0, `f64_min`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:225
assert_return(() => invoke($0, `f32_max`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:225:59
assert_return(() => invoke($0, `f64_max`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:226
assert_return(() => invoke($0, `f32_store`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:226:59
assert_return(() => invoke($0, `f64_store`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:227
assert_return(() => invoke($0, `f32_call`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:227:59
assert_return(() => invoke($0, `f64_call`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:228
assert_return(() => invoke($0, `f32_call_indirect`, []), [value("i32", 66052)]);

// ./test/core/left-to-right.wast:229
assert_return(() => invoke($0, `f64_call_indirect`, []), [value("i32", 66052)]);

// ./test/core/left-to-right.wast:230
assert_return(() => invoke($0, `f32_select`, []), [value("i32", 66053)]);

// ./test/core/left-to-right.wast:230:61
assert_return(() => invoke($0, `f64_select`, []), [value("i32", 66053)]);

// ./test/core/left-to-right.wast:232
assert_return(() => invoke($0, `br_if`, []), [value("i32", 258)]);

// ./test/core/left-to-right.wast:233
assert_return(() => invoke($0, `br_table`, []), [value("i32", 258)]);
