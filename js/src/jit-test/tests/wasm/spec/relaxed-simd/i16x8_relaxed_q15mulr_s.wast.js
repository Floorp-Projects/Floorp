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

// ./test/core/relaxed-simd/i16x8_relaxed_q15mulr_s.wast

// ./test/core/relaxed-simd/i16x8_relaxed_q15mulr_s.wast:4
let $0 = instantiate(`(module
    (func (export "i16x8.relaxed_q15mulr_s") (param v128 v128) (result v128) (i16x8.relaxed_q15mulr_s (local.get 0) (local.get 1)))

    (func (export "i16x8.relaxed_q15mulr_s_cmp") (param v128 v128) (result v128)
          (i16x8.eq
            (i16x8.relaxed_q15mulr_s (local.get 0) (local.get 1))
            (i16x8.relaxed_q15mulr_s (local.get 0) (local.get 1))))
)`);

// ./test/core/relaxed-simd/i16x8_relaxed_q15mulr_s.wast:14
assert_return(
  () => invoke($0, `i16x8.relaxed_q15mulr_s`, [
    i16x8([0x8000, 0x8001, 0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0]),
    i16x8([0x8000, 0x8000, 0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [
    either(
      i16x8([0x8000, 0x7fff, 0x7ffe, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x7fff, 0x7fff, 0x7ffe, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ),
  ],
);

// ./test/core/relaxed-simd/i16x8_relaxed_q15mulr_s.wast:23
assert_return(
  () => invoke($0, `i16x8.relaxed_q15mulr_s_cmp`, [
    i16x8([0x8000, 0x8001, 0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0]),
    i16x8([0x8000, 0x8000, 0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);
