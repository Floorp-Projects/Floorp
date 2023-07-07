// |jit-test| skip-if: !wasmSimdEnabled()

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

// ./test/core/simd/simd_memory-multi.wast

// ./test/core/simd/simd_memory-multi.wast:5
let $0 = instantiate(`(module
  (memory 1)
  (memory $$m 1)

  (func
    (local $$v v128)

    (drop (v128.load8_lane 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 offset=0 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 offset=0 align=1 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 align=1 1 (i32.const 0) (local.get $$v)))

    (drop (v128.load8_lane $$m 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane $$m offset=0 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane $$m offset=0 align=1 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane $$m align=1 1 (i32.const 0) (local.get $$v)))

    (drop (v128.load8_lane 1 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 offset=0 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 offset=0 align=1 1 (i32.const 0) (local.get $$v)))
    (drop (v128.load8_lane 1 align=1 1 (i32.const 0) (local.get $$v)))

    (v128.store8_lane 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane offset=0 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane offset=0 align=1 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane align=1 1 (i32.const 0) (local.get $$v))

    (v128.store8_lane $$m 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane $$m offset=0 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane $$m offset=0 align=1 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane $$m align=1 1 (i32.const 0) (local.get $$v))

    (v128.store8_lane 1 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane 1 offset=0 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane 1 offset=0 align=1 1 (i32.const 0) (local.get $$v))
    (v128.store8_lane 1 align=1 1 (i32.const 0) (local.get $$v))
  )
)`);
