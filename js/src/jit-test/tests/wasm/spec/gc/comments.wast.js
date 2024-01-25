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

// ./test/core/comments.wast

// ./test/core/comments.wast:10:0
let $0 = instantiate(`(module;;comment
)`);

// ./test/core/comments.wast:57:11
let $1 = instantiate(`(module(;comment;)
(;comment;))`);

// ./test/core/comments.wast:67
let $2 = instantiate(`(module
  (;comment(;nested(;further;)nested;)comment;)
)`);

// ./test/core/comments.wast:76
let $3 = instantiate(`(module
  (;comment;;comment(;nested;)comment;)
)`);

// ./test/core/comments.wast:83:8
let $4 = instantiate(`(func (export "f1") (result i32)   (i32.const 1)   ;; comment
   (return (i32.const 2)) 
 ) (func (export "f2") (result i32)   (i32.const 1)   ;; comment   (return (i32.const 2)) 
 ) (func (export "f3") (result i32)   (i32.const 1)   ;; comment
   (return (i32.const 2)) 
 ) `);

// ./test/core/comments.wast:104
assert_return(() => invoke($4, `f1`, []), [value("i32", 2)]);

// ./test/core/comments.wast:105
assert_return(() => invoke($4, `f2`, []), [value("i32", 2)]);

// ./test/core/comments.wast:106
assert_return(() => invoke($4, `f3`, []), [value("i32", 2)]);
