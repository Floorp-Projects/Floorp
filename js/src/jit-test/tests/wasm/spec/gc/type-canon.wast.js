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

// ./test/core/type-canon.wast

// ./test/core/type-canon.wast:1
let $0 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t3))))
    (type $$t2 (func (param i32 (ref $$t1))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )
)`);

// ./test/core/type-canon.wast:9
let $1 = instantiate(`(module
  (rec
    (type $$t0 (func (param i32 (ref $$t2) (ref $$t3))))
    (type $$t1 (func (param i32 (ref $$t0) i32 (ref $$t4))))
    (type $$t2 (func (param i32 (ref $$t2) (ref $$t1))))
    (type $$t3 (func (param i32 (ref $$t2) i32 (ref $$t4))))
    (type $$t4 (func (param (ref $$t0) (ref $$t2))))
  )
)`);
