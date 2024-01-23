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

// ./test/core/obsolete-keywords.wast

// ./test/core/obsolete-keywords.wast:2
assert_malformed(
  () => instantiate(`(memory 1) (func (drop (current_memory))) `),
  `unknown operator current_memory`,
);

// ./test/core/obsolete-keywords.wast:10
assert_malformed(
  () => instantiate(`(memory 1) (func (drop (grow_memory (i32.const 0)))) `),
  `unknown operator grow_memory`,
);

// ./test/core/obsolete-keywords.wast:19
assert_malformed(
  () => instantiate(`(func (local $$i i32) (drop (get_local $$i))) `),
  `unknown operator get_local`,
);

// ./test/core/obsolete-keywords.wast:26
assert_malformed(
  () => instantiate(`(func (local $$i i32) (set_local $$i (i32.const 0))) `),
  `unknown operator set_local`,
);

// ./test/core/obsolete-keywords.wast:33
assert_malformed(
  () => instantiate(`(func (local $$i i32) (drop (tee_local $$i (i32.const 0)))) `),
  `unknown operator tee_local`,
);

// ./test/core/obsolete-keywords.wast:40
assert_malformed(
  () => instantiate(`(global $$g anyfunc (ref.null func)) `),
  `unknown operator anyfunc`,
);

// ./test/core/obsolete-keywords.wast:47
assert_malformed(
  () => instantiate(`(global $$g i32 (i32.const 0)) (func (drop (get_global $$g))) `),
  `unknown operator get_global`,
);

// ./test/core/obsolete-keywords.wast:55
assert_malformed(
  () => instantiate(`(global $$g (mut i32) (i32.const 0)) (func (set_global $$g (i32.const 0))) `),
  `unknown operator set_global`,
);

// ./test/core/obsolete-keywords.wast:63
assert_malformed(
  () => instantiate(`(func (drop (i32.wrap/i64 (i64.const 0)))) `),
  `unknown operator i32.wrap/i64`,
);

// ./test/core/obsolete-keywords.wast:70
assert_malformed(
  () => instantiate(`(func (drop (i32.trunc_s:sat/f32 (f32.const 0)))) `),
  `unknown operator i32.trunc_s:sat/f32`,
);

// ./test/core/obsolete-keywords.wast:77
assert_malformed(
  () => instantiate(`(func (drop (f32x4.convert_s/i32x4 (v128.const i64x2 0 0)))) `),
  `unknown operator f32x4.convert_s/i32x4`,
);
