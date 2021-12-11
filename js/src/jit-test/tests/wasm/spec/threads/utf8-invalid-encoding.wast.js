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

// ./test/core/utf8-invalid-encoding.wast

// ./test/core/utf8-invalid-encoding.wast:1
assert_malformed(
  () => instantiate(`(func (export "\\00\\00\\fe\\ff")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:2
assert_malformed(
  () => instantiate(`(func (export "\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:3
assert_malformed(
  () => instantiate(`(func (export "\\8f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:4
assert_malformed(
  () => instantiate(`(func (export "\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:5
assert_malformed(
  () => instantiate(`(func (export "\\9f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:6
assert_malformed(
  () => instantiate(`(func (export "\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:7
assert_malformed(
  () => instantiate(`(func (export "\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:8
assert_malformed(
  () => instantiate(`(func (export "\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:9
assert_malformed(
  () => instantiate(`(func (export "\\c0\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:10
assert_malformed(
  () => instantiate(`(func (export "\\c1\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:11
assert_malformed(
  () => instantiate(`(func (export "\\c1\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:12
assert_malformed(
  () => instantiate(`(func (export "\\c2\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:13
assert_malformed(
  () => instantiate(`(func (export "\\c2\\2e")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:14
assert_malformed(
  () => instantiate(`(func (export "\\c2\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:15
assert_malformed(
  () => instantiate(`(func (export "\\c2\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:16
assert_malformed(
  () => instantiate(`(func (export "\\c2\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:17
assert_malformed(
  () => instantiate(`(func (export "\\c2")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:18
assert_malformed(
  () => instantiate(`(func (export "\\c2\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:19
assert_malformed(
  () => instantiate(`(func (export "\\df\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:20
assert_malformed(
  () => instantiate(`(func (export "\\df\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:21
assert_malformed(
  () => instantiate(`(func (export "\\df\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:22
assert_malformed(
  () => instantiate(`(func (export "\\df\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:23
assert_malformed(
  () => instantiate(`(func (export "\\e0\\00\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:24
assert_malformed(
  () => instantiate(`(func (export "\\e0\\7f\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:25
assert_malformed(
  () => instantiate(`(func (export "\\e0\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:26
assert_malformed(
  () => instantiate(`(func (export "\\e0\\80\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:27
assert_malformed(
  () => instantiate(`(func (export "\\e0\\9f\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:28
assert_malformed(
  () => instantiate(`(func (export "\\e0\\9f\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:29
assert_malformed(
  () => instantiate(`(func (export "\\e0\\a0\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:30
assert_malformed(
  () => instantiate(`(func (export "\\e0\\a0\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:31
assert_malformed(
  () => instantiate(`(func (export "\\e0\\a0\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:32
assert_malformed(
  () => instantiate(`(func (export "\\e0\\a0\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:33
assert_malformed(
  () => instantiate(`(func (export "\\e0\\c0\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:34
assert_malformed(
  () => instantiate(`(func (export "\\e0\\fd\\a0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:35
assert_malformed(
  () => instantiate(`(func (export "\\e1\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:36
assert_malformed(
  () => instantiate(`(func (export "\\e1\\2e")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:37
assert_malformed(
  () => instantiate(`(func (export "\\e1\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:38
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:39
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\2e")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:40
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:41
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:42
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:43
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:44
assert_malformed(
  () => instantiate(`(func (export "\\e1\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:45
assert_malformed(
  () => instantiate(`(func (export "\\e1\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:46
assert_malformed(
  () => instantiate(`(func (export "\\e1")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:47
assert_malformed(
  () => instantiate(`(func (export "\\e1\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:48
assert_malformed(
  () => instantiate(`(func (export "\\ec\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:49
assert_malformed(
  () => instantiate(`(func (export "\\ec\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:50
assert_malformed(
  () => instantiate(`(func (export "\\ec\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:51
assert_malformed(
  () => instantiate(`(func (export "\\ec\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:52
assert_malformed(
  () => instantiate(`(func (export "\\ec\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:53
assert_malformed(
  () => instantiate(`(func (export "\\ec\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:54
assert_malformed(
  () => instantiate(`(func (export "\\ec\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:55
assert_malformed(
  () => instantiate(`(func (export "\\ec\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:56
assert_malformed(
  () => instantiate(`(func (export "\\ed\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:57
assert_malformed(
  () => instantiate(`(func (export "\\ed\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:58
assert_malformed(
  () => instantiate(`(func (export "\\ed\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:59
assert_malformed(
  () => instantiate(`(func (export "\\ed\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:60
assert_malformed(
  () => instantiate(`(func (export "\\ed\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:61
assert_malformed(
  () => instantiate(`(func (export "\\ed\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:62
assert_malformed(
  () => instantiate(`(func (export "\\ed\\a0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:63
assert_malformed(
  () => instantiate(`(func (export "\\ed\\a0\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:64
assert_malformed(
  () => instantiate(`(func (export "\\ed\\bf\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:65
assert_malformed(
  () => instantiate(`(func (export "\\ed\\bf\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:66
assert_malformed(
  () => instantiate(`(func (export "\\ed\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:67
assert_malformed(
  () => instantiate(`(func (export "\\ed\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:68
assert_malformed(
  () => instantiate(`(func (export "\\ee\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:69
assert_malformed(
  () => instantiate(`(func (export "\\ee\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:70
assert_malformed(
  () => instantiate(`(func (export "\\ee\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:71
assert_malformed(
  () => instantiate(`(func (export "\\ee\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:72
assert_malformed(
  () => instantiate(`(func (export "\\ee\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:73
assert_malformed(
  () => instantiate(`(func (export "\\ee\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:74
assert_malformed(
  () => instantiate(`(func (export "\\ee\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:75
assert_malformed(
  () => instantiate(`(func (export "\\ee\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:76
assert_malformed(
  () => instantiate(`(func (export "\\ef\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:77
assert_malformed(
  () => instantiate(`(func (export "\\ef\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:78
assert_malformed(
  () => instantiate(`(func (export "\\ef\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:79
assert_malformed(
  () => instantiate(`(func (export "\\ef\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:80
assert_malformed(
  () => instantiate(`(func (export "\\ef\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:81
assert_malformed(
  () => instantiate(`(func (export "\\ef\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:82
assert_malformed(
  () => instantiate(`(func (export "\\ef\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:83
assert_malformed(
  () => instantiate(`(func (export "\\ef\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:84
assert_malformed(
  () => instantiate(`(func (export "\\f0\\00\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:85
assert_malformed(
  () => instantiate(`(func (export "\\f0\\7f\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:86
assert_malformed(
  () => instantiate(`(func (export "\\f0\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:87
assert_malformed(
  () => instantiate(`(func (export "\\f0\\80\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:88
assert_malformed(
  () => instantiate(`(func (export "\\f0\\8f\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:89
assert_malformed(
  () => instantiate(`(func (export "\\f0\\8f\\bf\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:90
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\00\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:91
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\7f\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:92
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\90\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:93
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\90\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:94
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\90\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:95
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\90\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:96
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\c0\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:97
assert_malformed(
  () => instantiate(`(func (export "\\f0\\90\\fd\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:98
assert_malformed(
  () => instantiate(`(func (export "\\f0\\c0\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:99
assert_malformed(
  () => instantiate(`(func (export "\\f0\\fd\\90\\90")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:100
assert_malformed(
  () => instantiate(`(func (export "\\f1\\00\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:101
assert_malformed(
  () => instantiate(`(func (export "\\f1\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:102
assert_malformed(
  () => instantiate(`(func (export "\\f1\\7f\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:103
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:104
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:105
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:106
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:107
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:108
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:109
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:110
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:111
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:112
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:113
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:114
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:115
assert_malformed(
  () => instantiate(`(func (export "\\f1\\80\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:116
assert_malformed(
  () => instantiate(`(func (export "\\f1\\c0\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:117
assert_malformed(
  () => instantiate(`(func (export "\\f1")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:118
assert_malformed(
  () => instantiate(`(func (export "\\f1\\fd\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:119
assert_malformed(
  () => instantiate(`(func (export "\\f3\\00\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:120
assert_malformed(
  () => instantiate(`(func (export "\\f3\\7f\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:121
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:122
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:123
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:124
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:125
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:126
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:127
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:128
assert_malformed(
  () => instantiate(`(func (export "\\f3\\80\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:129
assert_malformed(
  () => instantiate(`(func (export "\\f3\\c0\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:130
assert_malformed(
  () => instantiate(`(func (export "\\f3\\fd\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:131
assert_malformed(
  () => instantiate(`(func (export "\\f4\\00\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:132
assert_malformed(
  () => instantiate(`(func (export "\\f4\\7f\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:133
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\00\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:134
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\7f\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:135
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\80\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:136
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\80\\7f")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:137
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\80\\c0")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:138
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\80\\fd")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:139
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\c0\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:140
assert_malformed(
  () => instantiate(`(func (export "\\f4\\80\\fd\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:141
assert_malformed(
  () => instantiate(`(func (export "\\f4\\90\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:142
assert_malformed(
  () => instantiate(`(func (export "\\f4\\bf\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:143
assert_malformed(
  () => instantiate(`(func (export "\\f4\\c0\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:144
assert_malformed(
  () => instantiate(`(func (export "\\f4\\fd\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:145
assert_malformed(
  () => instantiate(`(func (export "\\f5\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:146
assert_malformed(
  () => instantiate(`(func (export "\\f7\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:147
assert_malformed(
  () => instantiate(`(func (export "\\f7\\bf\\bf\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:148
assert_malformed(
  () => instantiate(`(func (export "\\f8\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:149
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:150
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:151
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:152
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:153
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:154
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:155
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:156
assert_malformed(
  () => instantiate(`(func (export "\\f8\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:157
assert_malformed(
  () => instantiate(`(func (export "\\f8")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:158
assert_malformed(
  () => instantiate(`(func (export "\\fb\\bf\\bf\\bf\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:159
assert_malformed(
  () => instantiate(`(func (export "\\fc\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:160
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:161
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:162
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:163
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80\\80\\23")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:164
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:165
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:166
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:167
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:168
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:169
assert_malformed(
  () => instantiate(`(func (export "\\fc\\80")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:170
assert_malformed(
  () => instantiate(`(func (export "\\fc")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:171
assert_malformed(
  () => instantiate(`(func (export "\\fd\\bf\\bf\\bf\\bf\\bf")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:172
assert_malformed(
  () => instantiate(`(func (export "\\fe")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:173
assert_malformed(
  () => instantiate(`(func (export "\\fe\\ff")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:174
assert_malformed(
  () => instantiate(`(func (export "\\ff")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:175
assert_malformed(
  () => instantiate(`(func (export "\\ff\\fe\\00\\00")) `),
  `invalid UTF-8 encoding`,
);

// ./test/core/utf8-invalid-encoding.wast:176
assert_malformed(
  () => instantiate(`(func (export "\\ff\\fe")) `),
  `invalid UTF-8 encoding`,
);
