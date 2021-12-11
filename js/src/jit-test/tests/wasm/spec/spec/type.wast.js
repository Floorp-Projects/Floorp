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

// ./test/core/type.wast

// ./test/core/type.wast:3
let $0 = instantiate(`(module
  (type (func))
  (type $$t (func))

  (type (func (param i32)))
  (type (func (param $$x i32)))
  (type (func (result i32)))
  (type (func (param i32) (result i32)))
  (type (func (param $$x i32) (result i32)))

  (type (func (param f32 f64)))
  (type (func (result i64 f32)))
  (type (func (param i32 i64) (result f32 f64)))

  (type (func (param f32) (param f64)))
  (type (func (param $$x f32) (param f64)))
  (type (func (param f32) (param $$y f64)))
  (type (func (param $$x f32) (param $$y f64)))
  (type (func (result i64) (result f32)))
  (type (func (param i32) (param i64) (result f32) (result f64)))
  (type (func (param $$x i32) (param $$y i64) (result f32) (result f64)))

  (type (func (param f32 f64) (param $$x i32) (param f64 i32 i32)))
  (type (func (result i64 i64 f32) (result f32 i32)))
  (type
    (func (param i32 i32) (param i64 i32) (result f32 f64) (result f64 i32))
  )

  (type (func (param) (param $$x f32) (param) (param) (param f64 i32) (param)))
  (type
    (func (result) (result) (result i64 i64) (result) (result f32) (result))
  )
  (type
    (func
      (param i32 i32) (param i64 i32) (param) (param $$x i32) (param)
      (result) (result f32 f64) (result f64 i32) (result)
    )
  )
)`);

// ./test/core/type.wast:43
assert_malformed(
  () => instantiate(`(type (func (result i32) (param i32))) `),
  `result before parameter`,
);

// ./test/core/type.wast:47
assert_malformed(
  () => instantiate(`(type (func (result $$x i32))) `),
  `unexpected token`,
);
