
// table_copy.wast:5
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x85\x80\x80\x80\x00\x01\x60\x00\x01\x7f\x03\x86\x80\x80\x80\x00\x05\x00\x00\x00\x00\x00\x07\x9f\x80\x80\x80\x00\x05\x03\x65\x66\x30\x00\x00\x03\x65\x66\x31\x00\x01\x03\x65\x66\x32\x00\x02\x03\x65\x66\x33\x00\x03\x03\x65\x66\x34\x00\x04\x0a\xae\x80\x80\x80\x00\x05\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b");

// table_copy.wast:12
register("a", $1)

// table_copy.wast:14
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xc2\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x83\x80\x80\x80\x00\x00\x01\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:39
run(() => call($2, "test", []));

// table_copy.wast:40
assert_trap(() => call($2, "check", [0]));

// table_copy.wast:41
assert_trap(() => call($2, "check", [1]));

// table_copy.wast:42
assert_return(() => call($2, "check", [2]), 3);

// table_copy.wast:43
assert_return(() => call($2, "check", [3]), 1);

// table_copy.wast:44
assert_return(() => call($2, "check", [4]), 4);

// table_copy.wast:45
assert_return(() => call($2, "check", [5]), 1);

// table_copy.wast:46
assert_trap(() => call($2, "check", [6]));

// table_copy.wast:47
assert_trap(() => call($2, "check", [7]));

// table_copy.wast:48
assert_trap(() => call($2, "check", [8]));

// table_copy.wast:49
assert_trap(() => call($2, "check", [9]));

// table_copy.wast:50
assert_trap(() => call($2, "check", [10]));

// table_copy.wast:51
assert_trap(() => call($2, "check", [11]));

// table_copy.wast:52
assert_return(() => call($2, "check", [12]), 7);

// table_copy.wast:53
assert_return(() => call($2, "check", [13]), 5);

// table_copy.wast:54
assert_return(() => call($2, "check", [14]), 2);

// table_copy.wast:55
assert_return(() => call($2, "check", [15]), 3);

// table_copy.wast:56
assert_return(() => call($2, "check", [16]), 6);

// table_copy.wast:57
assert_trap(() => call($2, "check", [17]));

// table_copy.wast:58
assert_trap(() => call($2, "check", [18]));

// table_copy.wast:59
assert_trap(() => call($2, "check", [19]));

// table_copy.wast:60
assert_trap(() => call($2, "check", [20]));

// table_copy.wast:61
assert_trap(() => call($2, "check", [21]));

// table_copy.wast:62
assert_trap(() => call($2, "check", [22]));

// table_copy.wast:63
assert_trap(() => call($2, "check", [23]));

// table_copy.wast:64
assert_trap(() => call($2, "check", [24]));

// table_copy.wast:65
assert_trap(() => call($2, "check", [25]));

// table_copy.wast:66
assert_trap(() => call($2, "check", [26]));

// table_copy.wast:67
assert_trap(() => call($2, "check", [27]));

// table_copy.wast:68
assert_trap(() => call($2, "check", [28]));

// table_copy.wast:69
assert_trap(() => call($2, "check", [29]));

// table_copy.wast:71
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0d\x41\x02\x41\x03\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:96
run(() => call($3, "test", []));

// table_copy.wast:97
assert_trap(() => call($3, "check", [0]));

// table_copy.wast:98
assert_trap(() => call($3, "check", [1]));

// table_copy.wast:99
assert_return(() => call($3, "check", [2]), 3);

// table_copy.wast:100
assert_return(() => call($3, "check", [3]), 1);

// table_copy.wast:101
assert_return(() => call($3, "check", [4]), 4);

// table_copy.wast:102
assert_return(() => call($3, "check", [5]), 1);

// table_copy.wast:103
assert_trap(() => call($3, "check", [6]));

// table_copy.wast:104
assert_trap(() => call($3, "check", [7]));

// table_copy.wast:105
assert_trap(() => call($3, "check", [8]));

// table_copy.wast:106
assert_trap(() => call($3, "check", [9]));

// table_copy.wast:107
assert_trap(() => call($3, "check", [10]));

// table_copy.wast:108
assert_trap(() => call($3, "check", [11]));

// table_copy.wast:109
assert_return(() => call($3, "check", [12]), 7);

// table_copy.wast:110
assert_return(() => call($3, "check", [13]), 3);

// table_copy.wast:111
assert_return(() => call($3, "check", [14]), 1);

// table_copy.wast:112
assert_return(() => call($3, "check", [15]), 4);

// table_copy.wast:113
assert_return(() => call($3, "check", [16]), 6);

// table_copy.wast:114
assert_trap(() => call($3, "check", [17]));

// table_copy.wast:115
assert_trap(() => call($3, "check", [18]));

// table_copy.wast:116
assert_trap(() => call($3, "check", [19]));

// table_copy.wast:117
assert_trap(() => call($3, "check", [20]));

// table_copy.wast:118
assert_trap(() => call($3, "check", [21]));

// table_copy.wast:119
assert_trap(() => call($3, "check", [22]));

// table_copy.wast:120
assert_trap(() => call($3, "check", [23]));

// table_copy.wast:121
assert_trap(() => call($3, "check", [24]));

// table_copy.wast:122
assert_trap(() => call($3, "check", [25]));

// table_copy.wast:123
assert_trap(() => call($3, "check", [26]));

// table_copy.wast:124
assert_trap(() => call($3, "check", [27]));

// table_copy.wast:125
assert_trap(() => call($3, "check", [28]));

// table_copy.wast:126
assert_trap(() => call($3, "check", [29]));

// table_copy.wast:128
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x19\x41\x0f\x41\x02\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:153
run(() => call($4, "test", []));

// table_copy.wast:154
assert_trap(() => call($4, "check", [0]));

// table_copy.wast:155
assert_trap(() => call($4, "check", [1]));

// table_copy.wast:156
assert_return(() => call($4, "check", [2]), 3);

// table_copy.wast:157
assert_return(() => call($4, "check", [3]), 1);

// table_copy.wast:158
assert_return(() => call($4, "check", [4]), 4);

// table_copy.wast:159
assert_return(() => call($4, "check", [5]), 1);

// table_copy.wast:160
assert_trap(() => call($4, "check", [6]));

// table_copy.wast:161
assert_trap(() => call($4, "check", [7]));

// table_copy.wast:162
assert_trap(() => call($4, "check", [8]));

// table_copy.wast:163
assert_trap(() => call($4, "check", [9]));

// table_copy.wast:164
assert_trap(() => call($4, "check", [10]));

// table_copy.wast:165
assert_trap(() => call($4, "check", [11]));

// table_copy.wast:166
assert_return(() => call($4, "check", [12]), 7);

// table_copy.wast:167
assert_return(() => call($4, "check", [13]), 5);

// table_copy.wast:168
assert_return(() => call($4, "check", [14]), 2);

// table_copy.wast:169
assert_return(() => call($4, "check", [15]), 3);

// table_copy.wast:170
assert_return(() => call($4, "check", [16]), 6);

// table_copy.wast:171
assert_trap(() => call($4, "check", [17]));

// table_copy.wast:172
assert_trap(() => call($4, "check", [18]));

// table_copy.wast:173
assert_trap(() => call($4, "check", [19]));

// table_copy.wast:174
assert_trap(() => call($4, "check", [20]));

// table_copy.wast:175
assert_trap(() => call($4, "check", [21]));

// table_copy.wast:176
assert_trap(() => call($4, "check", [22]));

// table_copy.wast:177
assert_trap(() => call($4, "check", [23]));

// table_copy.wast:178
assert_trap(() => call($4, "check", [24]));

// table_copy.wast:179
assert_return(() => call($4, "check", [25]), 3);

// table_copy.wast:180
assert_return(() => call($4, "check", [26]), 6);

// table_copy.wast:181
assert_trap(() => call($4, "check", [27]));

// table_copy.wast:182
assert_trap(() => call($4, "check", [28]));

// table_copy.wast:183
assert_trap(() => call($4, "check", [29]));

// table_copy.wast:185
let $5 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0d\x41\x19\x41\x03\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:210
run(() => call($5, "test", []));

// table_copy.wast:211
assert_trap(() => call($5, "check", [0]));

// table_copy.wast:212
assert_trap(() => call($5, "check", [1]));

// table_copy.wast:213
assert_return(() => call($5, "check", [2]), 3);

// table_copy.wast:214
assert_return(() => call($5, "check", [3]), 1);

// table_copy.wast:215
assert_return(() => call($5, "check", [4]), 4);

// table_copy.wast:216
assert_return(() => call($5, "check", [5]), 1);

// table_copy.wast:217
assert_trap(() => call($5, "check", [6]));

// table_copy.wast:218
assert_trap(() => call($5, "check", [7]));

// table_copy.wast:219
assert_trap(() => call($5, "check", [8]));

// table_copy.wast:220
assert_trap(() => call($5, "check", [9]));

// table_copy.wast:221
assert_trap(() => call($5, "check", [10]));

// table_copy.wast:222
assert_trap(() => call($5, "check", [11]));

// table_copy.wast:223
assert_return(() => call($5, "check", [12]), 7);

// table_copy.wast:224
assert_trap(() => call($5, "check", [13]));

// table_copy.wast:225
assert_trap(() => call($5, "check", [14]));

// table_copy.wast:226
assert_trap(() => call($5, "check", [15]));

// table_copy.wast:227
assert_return(() => call($5, "check", [16]), 6);

// table_copy.wast:228
assert_trap(() => call($5, "check", [17]));

// table_copy.wast:229
assert_trap(() => call($5, "check", [18]));

// table_copy.wast:230
assert_trap(() => call($5, "check", [19]));

// table_copy.wast:231
assert_trap(() => call($5, "check", [20]));

// table_copy.wast:232
assert_trap(() => call($5, "check", [21]));

// table_copy.wast:233
assert_trap(() => call($5, "check", [22]));

// table_copy.wast:234
assert_trap(() => call($5, "check", [23]));

// table_copy.wast:235
assert_trap(() => call($5, "check", [24]));

// table_copy.wast:236
assert_trap(() => call($5, "check", [25]));

// table_copy.wast:237
assert_trap(() => call($5, "check", [26]));

// table_copy.wast:238
assert_trap(() => call($5, "check", [27]));

// table_copy.wast:239
assert_trap(() => call($5, "check", [28]));

// table_copy.wast:240
assert_trap(() => call($5, "check", [29]));

// table_copy.wast:242
let $6 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x14\x41\x16\x41\x04\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:267
run(() => call($6, "test", []));

// table_copy.wast:268
assert_trap(() => call($6, "check", [0]));

// table_copy.wast:269
assert_trap(() => call($6, "check", [1]));

// table_copy.wast:270
assert_return(() => call($6, "check", [2]), 3);

// table_copy.wast:271
assert_return(() => call($6, "check", [3]), 1);

// table_copy.wast:272
assert_return(() => call($6, "check", [4]), 4);

// table_copy.wast:273
assert_return(() => call($6, "check", [5]), 1);

// table_copy.wast:274
assert_trap(() => call($6, "check", [6]));

// table_copy.wast:275
assert_trap(() => call($6, "check", [7]));

// table_copy.wast:276
assert_trap(() => call($6, "check", [8]));

// table_copy.wast:277
assert_trap(() => call($6, "check", [9]));

// table_copy.wast:278
assert_trap(() => call($6, "check", [10]));

// table_copy.wast:279
assert_trap(() => call($6, "check", [11]));

// table_copy.wast:280
assert_return(() => call($6, "check", [12]), 7);

// table_copy.wast:281
assert_return(() => call($6, "check", [13]), 5);

// table_copy.wast:282
assert_return(() => call($6, "check", [14]), 2);

// table_copy.wast:283
assert_return(() => call($6, "check", [15]), 3);

// table_copy.wast:284
assert_return(() => call($6, "check", [16]), 6);

// table_copy.wast:285
assert_trap(() => call($6, "check", [17]));

// table_copy.wast:286
assert_trap(() => call($6, "check", [18]));

// table_copy.wast:287
assert_trap(() => call($6, "check", [19]));

// table_copy.wast:288
assert_trap(() => call($6, "check", [20]));

// table_copy.wast:289
assert_trap(() => call($6, "check", [21]));

// table_copy.wast:290
assert_trap(() => call($6, "check", [22]));

// table_copy.wast:291
assert_trap(() => call($6, "check", [23]));

// table_copy.wast:292
assert_trap(() => call($6, "check", [24]));

// table_copy.wast:293
assert_trap(() => call($6, "check", [25]));

// table_copy.wast:294
assert_trap(() => call($6, "check", [26]));

// table_copy.wast:295
assert_trap(() => call($6, "check", [27]));

// table_copy.wast:296
assert_trap(() => call($6, "check", [28]));

// table_copy.wast:297
assert_trap(() => call($6, "check", [29]));

// table_copy.wast:299
let $7 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x19\x41\x01\x41\x03\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:324
run(() => call($7, "test", []));

// table_copy.wast:325
assert_trap(() => call($7, "check", [0]));

// table_copy.wast:326
assert_trap(() => call($7, "check", [1]));

// table_copy.wast:327
assert_return(() => call($7, "check", [2]), 3);

// table_copy.wast:328
assert_return(() => call($7, "check", [3]), 1);

// table_copy.wast:329
assert_return(() => call($7, "check", [4]), 4);

// table_copy.wast:330
assert_return(() => call($7, "check", [5]), 1);

// table_copy.wast:331
assert_trap(() => call($7, "check", [6]));

// table_copy.wast:332
assert_trap(() => call($7, "check", [7]));

// table_copy.wast:333
assert_trap(() => call($7, "check", [8]));

// table_copy.wast:334
assert_trap(() => call($7, "check", [9]));

// table_copy.wast:335
assert_trap(() => call($7, "check", [10]));

// table_copy.wast:336
assert_trap(() => call($7, "check", [11]));

// table_copy.wast:337
assert_return(() => call($7, "check", [12]), 7);

// table_copy.wast:338
assert_return(() => call($7, "check", [13]), 5);

// table_copy.wast:339
assert_return(() => call($7, "check", [14]), 2);

// table_copy.wast:340
assert_return(() => call($7, "check", [15]), 3);

// table_copy.wast:341
assert_return(() => call($7, "check", [16]), 6);

// table_copy.wast:342
assert_trap(() => call($7, "check", [17]));

// table_copy.wast:343
assert_trap(() => call($7, "check", [18]));

// table_copy.wast:344
assert_trap(() => call($7, "check", [19]));

// table_copy.wast:345
assert_trap(() => call($7, "check", [20]));

// table_copy.wast:346
assert_trap(() => call($7, "check", [21]));

// table_copy.wast:347
assert_trap(() => call($7, "check", [22]));

// table_copy.wast:348
assert_trap(() => call($7, "check", [23]));

// table_copy.wast:349
assert_trap(() => call($7, "check", [24]));

// table_copy.wast:350
assert_trap(() => call($7, "check", [25]));

// table_copy.wast:351
assert_return(() => call($7, "check", [26]), 3);

// table_copy.wast:352
assert_return(() => call($7, "check", [27]), 1);

// table_copy.wast:353
assert_trap(() => call($7, "check", [28]));

// table_copy.wast:354
assert_trap(() => call($7, "check", [29]));

// table_copy.wast:356
let $8 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0a\x41\x0c\x41\x07\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:381
run(() => call($8, "test", []));

// table_copy.wast:382
assert_trap(() => call($8, "check", [0]));

// table_copy.wast:383
assert_trap(() => call($8, "check", [1]));

// table_copy.wast:384
assert_return(() => call($8, "check", [2]), 3);

// table_copy.wast:385
assert_return(() => call($8, "check", [3]), 1);

// table_copy.wast:386
assert_return(() => call($8, "check", [4]), 4);

// table_copy.wast:387
assert_return(() => call($8, "check", [5]), 1);

// table_copy.wast:388
assert_trap(() => call($8, "check", [6]));

// table_copy.wast:389
assert_trap(() => call($8, "check", [7]));

// table_copy.wast:390
assert_trap(() => call($8, "check", [8]));

// table_copy.wast:391
assert_trap(() => call($8, "check", [9]));

// table_copy.wast:392
assert_return(() => call($8, "check", [10]), 7);

// table_copy.wast:393
assert_return(() => call($8, "check", [11]), 5);

// table_copy.wast:394
assert_return(() => call($8, "check", [12]), 2);

// table_copy.wast:395
assert_return(() => call($8, "check", [13]), 3);

// table_copy.wast:396
assert_return(() => call($8, "check", [14]), 6);

// table_copy.wast:397
assert_trap(() => call($8, "check", [15]));

// table_copy.wast:398
assert_trap(() => call($8, "check", [16]));

// table_copy.wast:399
assert_trap(() => call($8, "check", [17]));

// table_copy.wast:400
assert_trap(() => call($8, "check", [18]));

// table_copy.wast:401
assert_trap(() => call($8, "check", [19]));

// table_copy.wast:402
assert_trap(() => call($8, "check", [20]));

// table_copy.wast:403
assert_trap(() => call($8, "check", [21]));

// table_copy.wast:404
assert_trap(() => call($8, "check", [22]));

// table_copy.wast:405
assert_trap(() => call($8, "check", [23]));

// table_copy.wast:406
assert_trap(() => call($8, "check", [24]));

// table_copy.wast:407
assert_trap(() => call($8, "check", [25]));

// table_copy.wast:408
assert_trap(() => call($8, "check", [26]));

// table_copy.wast:409
assert_trap(() => call($8, "check", [27]));

// table_copy.wast:410
assert_trap(() => call($8, "check", [28]));

// table_copy.wast:411
assert_trap(() => call($8, "check", [29]));

// table_copy.wast:413
let $9 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x02\xa9\x80\x80\x80\x00\x05\x01\x61\x03\x65\x66\x30\x00\x00\x01\x61\x03\x65\x66\x31\x00\x00\x01\x61\x03\x65\x66\x32\x00\x00\x01\x61\x03\x65\x66\x33\x00\x00\x01\x61\x03\x65\x66\x34\x00\x00\x03\x88\x80\x80\x80\x00\x07\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x90\x80\x80\x80\x00\x02\x04\x74\x65\x73\x74\x00\x0a\x05\x63\x68\x65\x63\x6b\x00\x0b\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xcb\x80\x80\x80\x00\x07\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0c\x41\x0a\x41\x07\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// table_copy.wast:438
run(() => call($9, "test", []));

// table_copy.wast:439
assert_trap(() => call($9, "check", [0]));

// table_copy.wast:440
assert_trap(() => call($9, "check", [1]));

// table_copy.wast:441
assert_return(() => call($9, "check", [2]), 3);

// table_copy.wast:442
assert_return(() => call($9, "check", [3]), 1);

// table_copy.wast:443
assert_return(() => call($9, "check", [4]), 4);

// table_copy.wast:444
assert_return(() => call($9, "check", [5]), 1);

// table_copy.wast:445
assert_trap(() => call($9, "check", [6]));

// table_copy.wast:446
assert_trap(() => call($9, "check", [7]));

// table_copy.wast:447
assert_trap(() => call($9, "check", [8]));

// table_copy.wast:448
assert_trap(() => call($9, "check", [9]));

// table_copy.wast:449
assert_trap(() => call($9, "check", [10]));

// table_copy.wast:450
assert_trap(() => call($9, "check", [11]));

// table_copy.wast:451
assert_trap(() => call($9, "check", [12]));

// table_copy.wast:452
assert_trap(() => call($9, "check", [13]));

// table_copy.wast:453
assert_return(() => call($9, "check", [14]), 7);

// table_copy.wast:454
assert_return(() => call($9, "check", [15]), 5);

// table_copy.wast:455
assert_return(() => call($9, "check", [16]), 2);

// table_copy.wast:456
assert_return(() => call($9, "check", [17]), 3);

// table_copy.wast:457
assert_return(() => call($9, "check", [18]), 6);

// table_copy.wast:458
assert_trap(() => call($9, "check", [19]));

// table_copy.wast:459
assert_trap(() => call($9, "check", [20]));

// table_copy.wast:460
assert_trap(() => call($9, "check", [21]));

// table_copy.wast:461
assert_trap(() => call($9, "check", [22]));

// table_copy.wast:462
assert_trap(() => call($9, "check", [23]));

// table_copy.wast:463
assert_trap(() => call($9, "check", [24]));

// table_copy.wast:464
assert_trap(() => call($9, "check", [25]));

// table_copy.wast:465
assert_trap(() => call($9, "check", [26]));

// table_copy.wast:466
assert_trap(() => call($9, "check", [27]));

// table_copy.wast:467
assert_trap(() => call($9, "check", [28]));

// table_copy.wast:468
assert_trap(() => call($9, "check", [29]));

// table_copy.wast:470
let $10 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x1c\x41\x01\x41\x03\xfc\x0e\x00\x00\x0b");

// table_copy.wast:492
assert_trap(() => call($10, "test", []));

// table_copy.wast:494
let $11 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x7e\x41\x01\x41\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:516
assert_trap(() => call($11, "test", []));

// table_copy.wast:518
let $12 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0f\x41\x19\x41\x06\xfc\x0e\x00\x00\x0b");

// table_copy.wast:540
assert_trap(() => call($12, "test", []));

// table_copy.wast:542
let $13 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0f\x41\x7e\x41\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:564
assert_trap(() => call($13, "test", []));

// table_copy.wast:566
let $14 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0f\x41\x19\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:588
run(() => call($14, "test", []));

// table_copy.wast:590
let $15 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x1e\x41\x0f\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:612
run(() => call($15, "test", []));

// table_copy.wast:614
let $16 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x1f\x41\x0f\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:636
assert_trap(() => call($16, "test", []));

// table_copy.wast:638
let $17 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0f\x41\x1e\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:660
run(() => call($17, "test", []));

// table_copy.wast:662
let $18 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x0f\x41\x1f\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:684
assert_trap(() => call($18, "test", []));

// table_copy.wast:686
let $19 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x1e\x41\x1e\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:708
run(() => call($19, "test", []));

// table_copy.wast:710
let $20 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x00\x00\x03\x8c\x80\x80\x80\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x1e\x1e\x07\x88\x80\x80\x80\x00\x01\x04\x74\x65\x73\x74\x00\x0a\x09\xa3\x80\x80\x80\x00\x04\x00\x41\x02\x0b\x04\x03\x01\x04\x01\x01\x00\x04\x02\x07\x01\x08\x00\x41\x0c\x0b\x05\x07\x05\x02\x03\x06\x01\x00\x05\x05\x09\x02\x07\x06\x0a\xec\x80\x80\x80\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x8c\x80\x80\x80\x00\x00\x41\x1f\x41\x1f\x41\x00\xfc\x0e\x00\x00\x0b");

// table_copy.wast:732
assert_trap(() => call($20, "test", []));

// table_copy.wast:734
let $21 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:760
assert_trap(() => call($21, "run", [24, 0, 16]));

// table_copy.wast:762
assert_return(() => call($21, "test", [0]), 0);

// table_copy.wast:763
assert_return(() => call($21, "test", [1]), 1);

// table_copy.wast:764
assert_return(() => call($21, "test", [2]), 2);

// table_copy.wast:765
assert_return(() => call($21, "test", [3]), 3);

// table_copy.wast:766
assert_return(() => call($21, "test", [4]), 4);

// table_copy.wast:767
assert_return(() => call($21, "test", [5]), 5);

// table_copy.wast:768
assert_return(() => call($21, "test", [6]), 6);

// table_copy.wast:769
assert_return(() => call($21, "test", [7]), 7);

// table_copy.wast:770
assert_trap(() => call($21, "test", [8]));

// table_copy.wast:771
assert_trap(() => call($21, "test", [9]));

// table_copy.wast:772
assert_trap(() => call($21, "test", [10]));

// table_copy.wast:773
assert_trap(() => call($21, "test", [11]));

// table_copy.wast:774
assert_trap(() => call($21, "test", [12]));

// table_copy.wast:775
assert_trap(() => call($21, "test", [13]));

// table_copy.wast:776
assert_trap(() => call($21, "test", [14]));

// table_copy.wast:777
assert_trap(() => call($21, "test", [15]));

// table_copy.wast:778
assert_trap(() => call($21, "test", [16]));

// table_copy.wast:779
assert_trap(() => call($21, "test", [17]));

// table_copy.wast:780
assert_trap(() => call($21, "test", [18]));

// table_copy.wast:781
assert_trap(() => call($21, "test", [19]));

// table_copy.wast:782
assert_trap(() => call($21, "test", [20]));

// table_copy.wast:783
assert_trap(() => call($21, "test", [21]));

// table_copy.wast:784
assert_trap(() => call($21, "test", [22]));

// table_copy.wast:785
assert_trap(() => call($21, "test", [23]));

// table_copy.wast:786
assert_trap(() => call($21, "test", [24]));

// table_copy.wast:787
assert_trap(() => call($21, "test", [25]));

// table_copy.wast:788
assert_trap(() => call($21, "test", [26]));

// table_copy.wast:789
assert_trap(() => call($21, "test", [27]));

// table_copy.wast:790
assert_trap(() => call($21, "test", [28]));

// table_copy.wast:791
assert_trap(() => call($21, "test", [29]));

// table_copy.wast:792
assert_trap(() => call($21, "test", [30]));

// table_copy.wast:793
assert_trap(() => call($21, "test", [31]));

// table_copy.wast:795
let $22 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8f\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x09\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:821
assert_trap(() => call($22, "run", [23, 0, 15]));

// table_copy.wast:823
assert_return(() => call($22, "test", [0]), 0);

// table_copy.wast:824
assert_return(() => call($22, "test", [1]), 1);

// table_copy.wast:825
assert_return(() => call($22, "test", [2]), 2);

// table_copy.wast:826
assert_return(() => call($22, "test", [3]), 3);

// table_copy.wast:827
assert_return(() => call($22, "test", [4]), 4);

// table_copy.wast:828
assert_return(() => call($22, "test", [5]), 5);

// table_copy.wast:829
assert_return(() => call($22, "test", [6]), 6);

// table_copy.wast:830
assert_return(() => call($22, "test", [7]), 7);

// table_copy.wast:831
assert_return(() => call($22, "test", [8]), 8);

// table_copy.wast:832
assert_trap(() => call($22, "test", [9]));

// table_copy.wast:833
assert_trap(() => call($22, "test", [10]));

// table_copy.wast:834
assert_trap(() => call($22, "test", [11]));

// table_copy.wast:835
assert_trap(() => call($22, "test", [12]));

// table_copy.wast:836
assert_trap(() => call($22, "test", [13]));

// table_copy.wast:837
assert_trap(() => call($22, "test", [14]));

// table_copy.wast:838
assert_trap(() => call($22, "test", [15]));

// table_copy.wast:839
assert_trap(() => call($22, "test", [16]));

// table_copy.wast:840
assert_trap(() => call($22, "test", [17]));

// table_copy.wast:841
assert_trap(() => call($22, "test", [18]));

// table_copy.wast:842
assert_trap(() => call($22, "test", [19]));

// table_copy.wast:843
assert_trap(() => call($22, "test", [20]));

// table_copy.wast:844
assert_trap(() => call($22, "test", [21]));

// table_copy.wast:845
assert_trap(() => call($22, "test", [22]));

// table_copy.wast:846
assert_trap(() => call($22, "test", [23]));

// table_copy.wast:847
assert_trap(() => call($22, "test", [24]));

// table_copy.wast:848
assert_trap(() => call($22, "test", [25]));

// table_copy.wast:849
assert_trap(() => call($22, "test", [26]));

// table_copy.wast:850
assert_trap(() => call($22, "test", [27]));

// table_copy.wast:851
assert_trap(() => call($22, "test", [28]));

// table_copy.wast:852
assert_trap(() => call($22, "test", [29]));

// table_copy.wast:853
assert_trap(() => call($22, "test", [30]));

// table_copy.wast:854
assert_trap(() => call($22, "test", [31]));

// table_copy.wast:856
let $23 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x18\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:882
assert_trap(() => call($23, "run", [0, 24, 16]));

// table_copy.wast:884
assert_trap(() => call($23, "test", [0]));

// table_copy.wast:885
assert_trap(() => call($23, "test", [1]));

// table_copy.wast:886
assert_trap(() => call($23, "test", [2]));

// table_copy.wast:887
assert_trap(() => call($23, "test", [3]));

// table_copy.wast:888
assert_trap(() => call($23, "test", [4]));

// table_copy.wast:889
assert_trap(() => call($23, "test", [5]));

// table_copy.wast:890
assert_trap(() => call($23, "test", [6]));

// table_copy.wast:891
assert_trap(() => call($23, "test", [7]));

// table_copy.wast:892
assert_trap(() => call($23, "test", [8]));

// table_copy.wast:893
assert_trap(() => call($23, "test", [9]));

// table_copy.wast:894
assert_trap(() => call($23, "test", [10]));

// table_copy.wast:895
assert_trap(() => call($23, "test", [11]));

// table_copy.wast:896
assert_trap(() => call($23, "test", [12]));

// table_copy.wast:897
assert_trap(() => call($23, "test", [13]));

// table_copy.wast:898
assert_trap(() => call($23, "test", [14]));

// table_copy.wast:899
assert_trap(() => call($23, "test", [15]));

// table_copy.wast:900
assert_trap(() => call($23, "test", [16]));

// table_copy.wast:901
assert_trap(() => call($23, "test", [17]));

// table_copy.wast:902
assert_trap(() => call($23, "test", [18]));

// table_copy.wast:903
assert_trap(() => call($23, "test", [19]));

// table_copy.wast:904
assert_trap(() => call($23, "test", [20]));

// table_copy.wast:905
assert_trap(() => call($23, "test", [21]));

// table_copy.wast:906
assert_trap(() => call($23, "test", [22]));

// table_copy.wast:907
assert_trap(() => call($23, "test", [23]));

// table_copy.wast:908
assert_return(() => call($23, "test", [24]), 0);

// table_copy.wast:909
assert_return(() => call($23, "test", [25]), 1);

// table_copy.wast:910
assert_return(() => call($23, "test", [26]), 2);

// table_copy.wast:911
assert_return(() => call($23, "test", [27]), 3);

// table_copy.wast:912
assert_return(() => call($23, "test", [28]), 4);

// table_copy.wast:913
assert_return(() => call($23, "test", [29]), 5);

// table_copy.wast:914
assert_return(() => call($23, "test", [30]), 6);

// table_copy.wast:915
assert_return(() => call($23, "test", [31]), 7);

// table_copy.wast:917
let $24 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8f\x80\x80\x80\x00\x01\x00\x41\x17\x0b\x09\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:943
assert_trap(() => call($24, "run", [0, 23, 15]));

// table_copy.wast:945
assert_trap(() => call($24, "test", [0]));

// table_copy.wast:946
assert_trap(() => call($24, "test", [1]));

// table_copy.wast:947
assert_trap(() => call($24, "test", [2]));

// table_copy.wast:948
assert_trap(() => call($24, "test", [3]));

// table_copy.wast:949
assert_trap(() => call($24, "test", [4]));

// table_copy.wast:950
assert_trap(() => call($24, "test", [5]));

// table_copy.wast:951
assert_trap(() => call($24, "test", [6]));

// table_copy.wast:952
assert_trap(() => call($24, "test", [7]));

// table_copy.wast:953
assert_trap(() => call($24, "test", [8]));

// table_copy.wast:954
assert_trap(() => call($24, "test", [9]));

// table_copy.wast:955
assert_trap(() => call($24, "test", [10]));

// table_copy.wast:956
assert_trap(() => call($24, "test", [11]));

// table_copy.wast:957
assert_trap(() => call($24, "test", [12]));

// table_copy.wast:958
assert_trap(() => call($24, "test", [13]));

// table_copy.wast:959
assert_trap(() => call($24, "test", [14]));

// table_copy.wast:960
assert_trap(() => call($24, "test", [15]));

// table_copy.wast:961
assert_trap(() => call($24, "test", [16]));

// table_copy.wast:962
assert_trap(() => call($24, "test", [17]));

// table_copy.wast:963
assert_trap(() => call($24, "test", [18]));

// table_copy.wast:964
assert_trap(() => call($24, "test", [19]));

// table_copy.wast:965
assert_trap(() => call($24, "test", [20]));

// table_copy.wast:966
assert_trap(() => call($24, "test", [21]));

// table_copy.wast:967
assert_trap(() => call($24, "test", [22]));

// table_copy.wast:968
assert_return(() => call($24, "test", [23]), 0);

// table_copy.wast:969
assert_return(() => call($24, "test", [24]), 1);

// table_copy.wast:970
assert_return(() => call($24, "test", [25]), 2);

// table_copy.wast:971
assert_return(() => call($24, "test", [26]), 3);

// table_copy.wast:972
assert_return(() => call($24, "test", [27]), 4);

// table_copy.wast:973
assert_return(() => call($24, "test", [28]), 5);

// table_copy.wast:974
assert_return(() => call($24, "test", [29]), 6);

// table_copy.wast:975
assert_return(() => call($24, "test", [30]), 7);

// table_copy.wast:976
assert_return(() => call($24, "test", [31]), 8);

// table_copy.wast:978
let $25 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x0b\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1004
assert_trap(() => call($25, "run", [24, 11, 16]));

// table_copy.wast:1006
assert_trap(() => call($25, "test", [0]));

// table_copy.wast:1007
assert_trap(() => call($25, "test", [1]));

// table_copy.wast:1008
assert_trap(() => call($25, "test", [2]));

// table_copy.wast:1009
assert_trap(() => call($25, "test", [3]));

// table_copy.wast:1010
assert_trap(() => call($25, "test", [4]));

// table_copy.wast:1011
assert_trap(() => call($25, "test", [5]));

// table_copy.wast:1012
assert_trap(() => call($25, "test", [6]));

// table_copy.wast:1013
assert_trap(() => call($25, "test", [7]));

// table_copy.wast:1014
assert_trap(() => call($25, "test", [8]));

// table_copy.wast:1015
assert_trap(() => call($25, "test", [9]));

// table_copy.wast:1016
assert_trap(() => call($25, "test", [10]));

// table_copy.wast:1017
assert_return(() => call($25, "test", [11]), 0);

// table_copy.wast:1018
assert_return(() => call($25, "test", [12]), 1);

// table_copy.wast:1019
assert_return(() => call($25, "test", [13]), 2);

// table_copy.wast:1020
assert_return(() => call($25, "test", [14]), 3);

// table_copy.wast:1021
assert_return(() => call($25, "test", [15]), 4);

// table_copy.wast:1022
assert_return(() => call($25, "test", [16]), 5);

// table_copy.wast:1023
assert_return(() => call($25, "test", [17]), 6);

// table_copy.wast:1024
assert_return(() => call($25, "test", [18]), 7);

// table_copy.wast:1025
assert_trap(() => call($25, "test", [19]));

// table_copy.wast:1026
assert_trap(() => call($25, "test", [20]));

// table_copy.wast:1027
assert_trap(() => call($25, "test", [21]));

// table_copy.wast:1028
assert_trap(() => call($25, "test", [22]));

// table_copy.wast:1029
assert_trap(() => call($25, "test", [23]));

// table_copy.wast:1030
assert_trap(() => call($25, "test", [24]));

// table_copy.wast:1031
assert_trap(() => call($25, "test", [25]));

// table_copy.wast:1032
assert_trap(() => call($25, "test", [26]));

// table_copy.wast:1033
assert_trap(() => call($25, "test", [27]));

// table_copy.wast:1034
assert_trap(() => call($25, "test", [28]));

// table_copy.wast:1035
assert_trap(() => call($25, "test", [29]));

// table_copy.wast:1036
assert_trap(() => call($25, "test", [30]));

// table_copy.wast:1037
assert_trap(() => call($25, "test", [31]));

// table_copy.wast:1039
let $26 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x18\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1065
assert_trap(() => call($26, "run", [11, 24, 16]));

// table_copy.wast:1067
assert_trap(() => call($26, "test", [0]));

// table_copy.wast:1068
assert_trap(() => call($26, "test", [1]));

// table_copy.wast:1069
assert_trap(() => call($26, "test", [2]));

// table_copy.wast:1070
assert_trap(() => call($26, "test", [3]));

// table_copy.wast:1071
assert_trap(() => call($26, "test", [4]));

// table_copy.wast:1072
assert_trap(() => call($26, "test", [5]));

// table_copy.wast:1073
assert_trap(() => call($26, "test", [6]));

// table_copy.wast:1074
assert_trap(() => call($26, "test", [7]));

// table_copy.wast:1075
assert_trap(() => call($26, "test", [8]));

// table_copy.wast:1076
assert_trap(() => call($26, "test", [9]));

// table_copy.wast:1077
assert_trap(() => call($26, "test", [10]));

// table_copy.wast:1078
assert_trap(() => call($26, "test", [11]));

// table_copy.wast:1079
assert_trap(() => call($26, "test", [12]));

// table_copy.wast:1080
assert_trap(() => call($26, "test", [13]));

// table_copy.wast:1081
assert_trap(() => call($26, "test", [14]));

// table_copy.wast:1082
assert_trap(() => call($26, "test", [15]));

// table_copy.wast:1083
assert_trap(() => call($26, "test", [16]));

// table_copy.wast:1084
assert_trap(() => call($26, "test", [17]));

// table_copy.wast:1085
assert_trap(() => call($26, "test", [18]));

// table_copy.wast:1086
assert_trap(() => call($26, "test", [19]));

// table_copy.wast:1087
assert_trap(() => call($26, "test", [20]));

// table_copy.wast:1088
assert_trap(() => call($26, "test", [21]));

// table_copy.wast:1089
assert_trap(() => call($26, "test", [22]));

// table_copy.wast:1090
assert_trap(() => call($26, "test", [23]));

// table_copy.wast:1091
assert_return(() => call($26, "test", [24]), 0);

// table_copy.wast:1092
assert_return(() => call($26, "test", [25]), 1);

// table_copy.wast:1093
assert_return(() => call($26, "test", [26]), 2);

// table_copy.wast:1094
assert_return(() => call($26, "test", [27]), 3);

// table_copy.wast:1095
assert_return(() => call($26, "test", [28]), 4);

// table_copy.wast:1096
assert_return(() => call($26, "test", [29]), 5);

// table_copy.wast:1097
assert_return(() => call($26, "test", [30]), 6);

// table_copy.wast:1098
assert_return(() => call($26, "test", [31]), 7);

// table_copy.wast:1100
let $27 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x15\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1126
assert_trap(() => call($27, "run", [24, 21, 16]));

// table_copy.wast:1128
assert_trap(() => call($27, "test", [0]));

// table_copy.wast:1129
assert_trap(() => call($27, "test", [1]));

// table_copy.wast:1130
assert_trap(() => call($27, "test", [2]));

// table_copy.wast:1131
assert_trap(() => call($27, "test", [3]));

// table_copy.wast:1132
assert_trap(() => call($27, "test", [4]));

// table_copy.wast:1133
assert_trap(() => call($27, "test", [5]));

// table_copy.wast:1134
assert_trap(() => call($27, "test", [6]));

// table_copy.wast:1135
assert_trap(() => call($27, "test", [7]));

// table_copy.wast:1136
assert_trap(() => call($27, "test", [8]));

// table_copy.wast:1137
assert_trap(() => call($27, "test", [9]));

// table_copy.wast:1138
assert_trap(() => call($27, "test", [10]));

// table_copy.wast:1139
assert_trap(() => call($27, "test", [11]));

// table_copy.wast:1140
assert_trap(() => call($27, "test", [12]));

// table_copy.wast:1141
assert_trap(() => call($27, "test", [13]));

// table_copy.wast:1142
assert_trap(() => call($27, "test", [14]));

// table_copy.wast:1143
assert_trap(() => call($27, "test", [15]));

// table_copy.wast:1144
assert_trap(() => call($27, "test", [16]));

// table_copy.wast:1145
assert_trap(() => call($27, "test", [17]));

// table_copy.wast:1146
assert_trap(() => call($27, "test", [18]));

// table_copy.wast:1147
assert_trap(() => call($27, "test", [19]));

// table_copy.wast:1148
assert_trap(() => call($27, "test", [20]));

// table_copy.wast:1149
assert_return(() => call($27, "test", [21]), 0);

// table_copy.wast:1150
assert_return(() => call($27, "test", [22]), 1);

// table_copy.wast:1151
assert_return(() => call($27, "test", [23]), 2);

// table_copy.wast:1152
assert_return(() => call($27, "test", [24]), 3);

// table_copy.wast:1153
assert_return(() => call($27, "test", [25]), 4);

// table_copy.wast:1154
assert_return(() => call($27, "test", [26]), 5);

// table_copy.wast:1155
assert_return(() => call($27, "test", [27]), 6);

// table_copy.wast:1156
assert_return(() => call($27, "test", [28]), 7);

// table_copy.wast:1157
assert_trap(() => call($27, "test", [29]));

// table_copy.wast:1158
assert_trap(() => call($27, "test", [30]));

// table_copy.wast:1159
assert_trap(() => call($27, "test", [31]));

// table_copy.wast:1161
let $28 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x8e\x80\x80\x80\x00\x01\x00\x41\x18\x0b\x08\x00\x01\x02\x03\x04\x05\x06\x07\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1187
assert_trap(() => call($28, "run", [21, 24, 16]));

// table_copy.wast:1189
assert_trap(() => call($28, "test", [0]));

// table_copy.wast:1190
assert_trap(() => call($28, "test", [1]));

// table_copy.wast:1191
assert_trap(() => call($28, "test", [2]));

// table_copy.wast:1192
assert_trap(() => call($28, "test", [3]));

// table_copy.wast:1193
assert_trap(() => call($28, "test", [4]));

// table_copy.wast:1194
assert_trap(() => call($28, "test", [5]));

// table_copy.wast:1195
assert_trap(() => call($28, "test", [6]));

// table_copy.wast:1196
assert_trap(() => call($28, "test", [7]));

// table_copy.wast:1197
assert_trap(() => call($28, "test", [8]));

// table_copy.wast:1198
assert_trap(() => call($28, "test", [9]));

// table_copy.wast:1199
assert_trap(() => call($28, "test", [10]));

// table_copy.wast:1200
assert_trap(() => call($28, "test", [11]));

// table_copy.wast:1201
assert_trap(() => call($28, "test", [12]));

// table_copy.wast:1202
assert_trap(() => call($28, "test", [13]));

// table_copy.wast:1203
assert_trap(() => call($28, "test", [14]));

// table_copy.wast:1204
assert_trap(() => call($28, "test", [15]));

// table_copy.wast:1205
assert_trap(() => call($28, "test", [16]));

// table_copy.wast:1206
assert_trap(() => call($28, "test", [17]));

// table_copy.wast:1207
assert_trap(() => call($28, "test", [18]));

// table_copy.wast:1208
assert_trap(() => call($28, "test", [19]));

// table_copy.wast:1209
assert_trap(() => call($28, "test", [20]));

// table_copy.wast:1210
assert_trap(() => call($28, "test", [21]));

// table_copy.wast:1211
assert_trap(() => call($28, "test", [22]));

// table_copy.wast:1212
assert_trap(() => call($28, "test", [23]));

// table_copy.wast:1213
assert_return(() => call($28, "test", [24]), 0);

// table_copy.wast:1214
assert_return(() => call($28, "test", [25]), 1);

// table_copy.wast:1215
assert_return(() => call($28, "test", [26]), 2);

// table_copy.wast:1216
assert_return(() => call($28, "test", [27]), 3);

// table_copy.wast:1217
assert_return(() => call($28, "test", [28]), 4);

// table_copy.wast:1218
assert_return(() => call($28, "test", [29]), 5);

// table_copy.wast:1219
assert_return(() => call($28, "test", [30]), 6);

// table_copy.wast:1220
assert_return(() => call($28, "test", [31]), 7);

// table_copy.wast:1222
let $29 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x20\x40\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x91\x80\x80\x80\x00\x01\x00\x41\x15\x0b\x0b\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1248
assert_trap(() => call($29, "run", [21, 21, 16]));

// table_copy.wast:1250
assert_trap(() => call($29, "test", [0]));

// table_copy.wast:1251
assert_trap(() => call($29, "test", [1]));

// table_copy.wast:1252
assert_trap(() => call($29, "test", [2]));

// table_copy.wast:1253
assert_trap(() => call($29, "test", [3]));

// table_copy.wast:1254
assert_trap(() => call($29, "test", [4]));

// table_copy.wast:1255
assert_trap(() => call($29, "test", [5]));

// table_copy.wast:1256
assert_trap(() => call($29, "test", [6]));

// table_copy.wast:1257
assert_trap(() => call($29, "test", [7]));

// table_copy.wast:1258
assert_trap(() => call($29, "test", [8]));

// table_copy.wast:1259
assert_trap(() => call($29, "test", [9]));

// table_copy.wast:1260
assert_trap(() => call($29, "test", [10]));

// table_copy.wast:1261
assert_trap(() => call($29, "test", [11]));

// table_copy.wast:1262
assert_trap(() => call($29, "test", [12]));

// table_copy.wast:1263
assert_trap(() => call($29, "test", [13]));

// table_copy.wast:1264
assert_trap(() => call($29, "test", [14]));

// table_copy.wast:1265
assert_trap(() => call($29, "test", [15]));

// table_copy.wast:1266
assert_trap(() => call($29, "test", [16]));

// table_copy.wast:1267
assert_trap(() => call($29, "test", [17]));

// table_copy.wast:1268
assert_trap(() => call($29, "test", [18]));

// table_copy.wast:1269
assert_trap(() => call($29, "test", [19]));

// table_copy.wast:1270
assert_trap(() => call($29, "test", [20]));

// table_copy.wast:1271
assert_return(() => call($29, "test", [21]), 0);

// table_copy.wast:1272
assert_return(() => call($29, "test", [22]), 1);

// table_copy.wast:1273
assert_return(() => call($29, "test", [23]), 2);

// table_copy.wast:1274
assert_return(() => call($29, "test", [24]), 3);

// table_copy.wast:1275
assert_return(() => call($29, "test", [25]), 4);

// table_copy.wast:1276
assert_return(() => call($29, "test", [26]), 5);

// table_copy.wast:1277
assert_return(() => call($29, "test", [27]), 6);

// table_copy.wast:1278
assert_return(() => call($29, "test", [28]), 7);

// table_copy.wast:1279
assert_return(() => call($29, "test", [29]), 8);

// table_copy.wast:1280
assert_return(() => call($29, "test", [30]), 9);

// table_copy.wast:1281
assert_return(() => call($29, "test", [31]), 10);

// table_copy.wast:1283
let $30 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x87\x80\x80\x80\x00\x01\x70\x01\x80\x01\x80\x01\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x97\x80\x80\x80\x00\x01\x00\x41\xf0\x00\x0b\x10\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1309
assert_trap(() => call($30, "run", [0, 112, -32]));

// table_copy.wast:1311
assert_trap(() => call($30, "test", [0]));

// table_copy.wast:1312
assert_trap(() => call($30, "test", [1]));

// table_copy.wast:1313
assert_trap(() => call($30, "test", [2]));

// table_copy.wast:1314
assert_trap(() => call($30, "test", [3]));

// table_copy.wast:1315
assert_trap(() => call($30, "test", [4]));

// table_copy.wast:1316
assert_trap(() => call($30, "test", [5]));

// table_copy.wast:1317
assert_trap(() => call($30, "test", [6]));

// table_copy.wast:1318
assert_trap(() => call($30, "test", [7]));

// table_copy.wast:1319
assert_trap(() => call($30, "test", [8]));

// table_copy.wast:1320
assert_trap(() => call($30, "test", [9]));

// table_copy.wast:1321
assert_trap(() => call($30, "test", [10]));

// table_copy.wast:1322
assert_trap(() => call($30, "test", [11]));

// table_copy.wast:1323
assert_trap(() => call($30, "test", [12]));

// table_copy.wast:1324
assert_trap(() => call($30, "test", [13]));

// table_copy.wast:1325
assert_trap(() => call($30, "test", [14]));

// table_copy.wast:1326
assert_trap(() => call($30, "test", [15]));

// table_copy.wast:1327
assert_trap(() => call($30, "test", [16]));

// table_copy.wast:1328
assert_trap(() => call($30, "test", [17]));

// table_copy.wast:1329
assert_trap(() => call($30, "test", [18]));

// table_copy.wast:1330
assert_trap(() => call($30, "test", [19]));

// table_copy.wast:1331
assert_trap(() => call($30, "test", [20]));

// table_copy.wast:1332
assert_trap(() => call($30, "test", [21]));

// table_copy.wast:1333
assert_trap(() => call($30, "test", [22]));

// table_copy.wast:1334
assert_trap(() => call($30, "test", [23]));

// table_copy.wast:1335
assert_trap(() => call($30, "test", [24]));

// table_copy.wast:1336
assert_trap(() => call($30, "test", [25]));

// table_copy.wast:1337
assert_trap(() => call($30, "test", [26]));

// table_copy.wast:1338
assert_trap(() => call($30, "test", [27]));

// table_copy.wast:1339
assert_trap(() => call($30, "test", [28]));

// table_copy.wast:1340
assert_trap(() => call($30, "test", [29]));

// table_copy.wast:1341
assert_trap(() => call($30, "test", [30]));

// table_copy.wast:1342
assert_trap(() => call($30, "test", [31]));

// table_copy.wast:1343
assert_trap(() => call($30, "test", [32]));

// table_copy.wast:1344
assert_trap(() => call($30, "test", [33]));

// table_copy.wast:1345
assert_trap(() => call($30, "test", [34]));

// table_copy.wast:1346
assert_trap(() => call($30, "test", [35]));

// table_copy.wast:1347
assert_trap(() => call($30, "test", [36]));

// table_copy.wast:1348
assert_trap(() => call($30, "test", [37]));

// table_copy.wast:1349
assert_trap(() => call($30, "test", [38]));

// table_copy.wast:1350
assert_trap(() => call($30, "test", [39]));

// table_copy.wast:1351
assert_trap(() => call($30, "test", [40]));

// table_copy.wast:1352
assert_trap(() => call($30, "test", [41]));

// table_copy.wast:1353
assert_trap(() => call($30, "test", [42]));

// table_copy.wast:1354
assert_trap(() => call($30, "test", [43]));

// table_copy.wast:1355
assert_trap(() => call($30, "test", [44]));

// table_copy.wast:1356
assert_trap(() => call($30, "test", [45]));

// table_copy.wast:1357
assert_trap(() => call($30, "test", [46]));

// table_copy.wast:1358
assert_trap(() => call($30, "test", [47]));

// table_copy.wast:1359
assert_trap(() => call($30, "test", [48]));

// table_copy.wast:1360
assert_trap(() => call($30, "test", [49]));

// table_copy.wast:1361
assert_trap(() => call($30, "test", [50]));

// table_copy.wast:1362
assert_trap(() => call($30, "test", [51]));

// table_copy.wast:1363
assert_trap(() => call($30, "test", [52]));

// table_copy.wast:1364
assert_trap(() => call($30, "test", [53]));

// table_copy.wast:1365
assert_trap(() => call($30, "test", [54]));

// table_copy.wast:1366
assert_trap(() => call($30, "test", [55]));

// table_copy.wast:1367
assert_trap(() => call($30, "test", [56]));

// table_copy.wast:1368
assert_trap(() => call($30, "test", [57]));

// table_copy.wast:1369
assert_trap(() => call($30, "test", [58]));

// table_copy.wast:1370
assert_trap(() => call($30, "test", [59]));

// table_copy.wast:1371
assert_trap(() => call($30, "test", [60]));

// table_copy.wast:1372
assert_trap(() => call($30, "test", [61]));

// table_copy.wast:1373
assert_trap(() => call($30, "test", [62]));

// table_copy.wast:1374
assert_trap(() => call($30, "test", [63]));

// table_copy.wast:1375
assert_trap(() => call($30, "test", [64]));

// table_copy.wast:1376
assert_trap(() => call($30, "test", [65]));

// table_copy.wast:1377
assert_trap(() => call($30, "test", [66]));

// table_copy.wast:1378
assert_trap(() => call($30, "test", [67]));

// table_copy.wast:1379
assert_trap(() => call($30, "test", [68]));

// table_copy.wast:1380
assert_trap(() => call($30, "test", [69]));

// table_copy.wast:1381
assert_trap(() => call($30, "test", [70]));

// table_copy.wast:1382
assert_trap(() => call($30, "test", [71]));

// table_copy.wast:1383
assert_trap(() => call($30, "test", [72]));

// table_copy.wast:1384
assert_trap(() => call($30, "test", [73]));

// table_copy.wast:1385
assert_trap(() => call($30, "test", [74]));

// table_copy.wast:1386
assert_trap(() => call($30, "test", [75]));

// table_copy.wast:1387
assert_trap(() => call($30, "test", [76]));

// table_copy.wast:1388
assert_trap(() => call($30, "test", [77]));

// table_copy.wast:1389
assert_trap(() => call($30, "test", [78]));

// table_copy.wast:1390
assert_trap(() => call($30, "test", [79]));

// table_copy.wast:1391
assert_trap(() => call($30, "test", [80]));

// table_copy.wast:1392
assert_trap(() => call($30, "test", [81]));

// table_copy.wast:1393
assert_trap(() => call($30, "test", [82]));

// table_copy.wast:1394
assert_trap(() => call($30, "test", [83]));

// table_copy.wast:1395
assert_trap(() => call($30, "test", [84]));

// table_copy.wast:1396
assert_trap(() => call($30, "test", [85]));

// table_copy.wast:1397
assert_trap(() => call($30, "test", [86]));

// table_copy.wast:1398
assert_trap(() => call($30, "test", [87]));

// table_copy.wast:1399
assert_trap(() => call($30, "test", [88]));

// table_copy.wast:1400
assert_trap(() => call($30, "test", [89]));

// table_copy.wast:1401
assert_trap(() => call($30, "test", [90]));

// table_copy.wast:1402
assert_trap(() => call($30, "test", [91]));

// table_copy.wast:1403
assert_trap(() => call($30, "test", [92]));

// table_copy.wast:1404
assert_trap(() => call($30, "test", [93]));

// table_copy.wast:1405
assert_trap(() => call($30, "test", [94]));

// table_copy.wast:1406
assert_trap(() => call($30, "test", [95]));

// table_copy.wast:1407
assert_trap(() => call($30, "test", [96]));

// table_copy.wast:1408
assert_trap(() => call($30, "test", [97]));

// table_copy.wast:1409
assert_trap(() => call($30, "test", [98]));

// table_copy.wast:1410
assert_trap(() => call($30, "test", [99]));

// table_copy.wast:1411
assert_trap(() => call($30, "test", [100]));

// table_copy.wast:1412
assert_trap(() => call($30, "test", [101]));

// table_copy.wast:1413
assert_trap(() => call($30, "test", [102]));

// table_copy.wast:1414
assert_trap(() => call($30, "test", [103]));

// table_copy.wast:1415
assert_trap(() => call($30, "test", [104]));

// table_copy.wast:1416
assert_trap(() => call($30, "test", [105]));

// table_copy.wast:1417
assert_trap(() => call($30, "test", [106]));

// table_copy.wast:1418
assert_trap(() => call($30, "test", [107]));

// table_copy.wast:1419
assert_trap(() => call($30, "test", [108]));

// table_copy.wast:1420
assert_trap(() => call($30, "test", [109]));

// table_copy.wast:1421
assert_trap(() => call($30, "test", [110]));

// table_copy.wast:1422
assert_trap(() => call($30, "test", [111]));

// table_copy.wast:1423
assert_return(() => call($30, "test", [112]), 0);

// table_copy.wast:1424
assert_return(() => call($30, "test", [113]), 1);

// table_copy.wast:1425
assert_return(() => call($30, "test", [114]), 2);

// table_copy.wast:1426
assert_return(() => call($30, "test", [115]), 3);

// table_copy.wast:1427
assert_return(() => call($30, "test", [116]), 4);

// table_copy.wast:1428
assert_return(() => call($30, "test", [117]), 5);

// table_copy.wast:1429
assert_return(() => call($30, "test", [118]), 6);

// table_copy.wast:1430
assert_return(() => call($30, "test", [119]), 7);

// table_copy.wast:1431
assert_return(() => call($30, "test", [120]), 8);

// table_copy.wast:1432
assert_return(() => call($30, "test", [121]), 9);

// table_copy.wast:1433
assert_return(() => call($30, "test", [122]), 10);

// table_copy.wast:1434
assert_return(() => call($30, "test", [123]), 11);

// table_copy.wast:1435
assert_return(() => call($30, "test", [124]), 12);

// table_copy.wast:1436
assert_return(() => call($30, "test", [125]), 13);

// table_copy.wast:1437
assert_return(() => call($30, "test", [126]), 14);

// table_copy.wast:1438
assert_return(() => call($30, "test", [127]), 15);

// table_copy.wast:1440
let $31 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x03\x93\x80\x80\x80\x00\x12\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x04\x87\x80\x80\x80\x00\x01\x70\x01\x80\x01\x80\x01\x07\xe4\x80\x80\x80\x00\x12\x02\x66\x30\x00\x00\x02\x66\x31\x00\x01\x02\x66\x32\x00\x02\x02\x66\x33\x00\x03\x02\x66\x34\x00\x04\x02\x66\x35\x00\x05\x02\x66\x36\x00\x06\x02\x66\x37\x00\x07\x02\x66\x38\x00\x08\x02\x66\x39\x00\x09\x03\x66\x31\x30\x00\x0a\x03\x66\x31\x31\x00\x0b\x03\x66\x31\x32\x00\x0c\x03\x66\x31\x33\x00\x0d\x03\x66\x31\x34\x00\x0e\x03\x66\x31\x35\x00\x0f\x04\x74\x65\x73\x74\x00\x10\x03\x72\x75\x6e\x00\x11\x09\x96\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x10\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x0a\xae\x81\x80\x80\x00\x12\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x84\x80\x80\x80\x00\x00\x41\x03\x0b\x84\x80\x80\x80\x00\x00\x41\x04\x0b\x84\x80\x80\x80\x00\x00\x41\x05\x0b\x84\x80\x80\x80\x00\x00\x41\x06\x0b\x84\x80\x80\x80\x00\x00\x41\x07\x0b\x84\x80\x80\x80\x00\x00\x41\x08\x0b\x84\x80\x80\x80\x00\x00\x41\x09\x0b\x84\x80\x80\x80\x00\x00\x41\x0a\x0b\x84\x80\x80\x80\x00\x00\x41\x0b\x0b\x84\x80\x80\x80\x00\x00\x41\x0c\x0b\x84\x80\x80\x80\x00\x00\x41\x0d\x0b\x84\x80\x80\x80\x00\x00\x41\x0e\x0b\x84\x80\x80\x80\x00\x00\x41\x0f\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b");

// table_copy.wast:1466
assert_trap(() => call($31, "run", [112, 0, -32]));

// table_copy.wast:1468
assert_return(() => call($31, "test", [0]), 0);

// table_copy.wast:1469
assert_return(() => call($31, "test", [1]), 1);

// table_copy.wast:1470
assert_return(() => call($31, "test", [2]), 2);

// table_copy.wast:1471
assert_return(() => call($31, "test", [3]), 3);

// table_copy.wast:1472
assert_return(() => call($31, "test", [4]), 4);

// table_copy.wast:1473
assert_return(() => call($31, "test", [5]), 5);

// table_copy.wast:1474
assert_return(() => call($31, "test", [6]), 6);

// table_copy.wast:1475
assert_return(() => call($31, "test", [7]), 7);

// table_copy.wast:1476
assert_return(() => call($31, "test", [8]), 8);

// table_copy.wast:1477
assert_return(() => call($31, "test", [9]), 9);

// table_copy.wast:1478
assert_return(() => call($31, "test", [10]), 10);

// table_copy.wast:1479
assert_return(() => call($31, "test", [11]), 11);

// table_copy.wast:1480
assert_return(() => call($31, "test", [12]), 12);

// table_copy.wast:1481
assert_return(() => call($31, "test", [13]), 13);

// table_copy.wast:1482
assert_return(() => call($31, "test", [14]), 14);

// table_copy.wast:1483
assert_return(() => call($31, "test", [15]), 15);

// table_copy.wast:1484
assert_trap(() => call($31, "test", [16]));

// table_copy.wast:1485
assert_trap(() => call($31, "test", [17]));

// table_copy.wast:1486
assert_trap(() => call($31, "test", [18]));

// table_copy.wast:1487
assert_trap(() => call($31, "test", [19]));

// table_copy.wast:1488
assert_trap(() => call($31, "test", [20]));

// table_copy.wast:1489
assert_trap(() => call($31, "test", [21]));

// table_copy.wast:1490
assert_trap(() => call($31, "test", [22]));

// table_copy.wast:1491
assert_trap(() => call($31, "test", [23]));

// table_copy.wast:1492
assert_trap(() => call($31, "test", [24]));

// table_copy.wast:1493
assert_trap(() => call($31, "test", [25]));

// table_copy.wast:1494
assert_trap(() => call($31, "test", [26]));

// table_copy.wast:1495
assert_trap(() => call($31, "test", [27]));

// table_copy.wast:1496
assert_trap(() => call($31, "test", [28]));

// table_copy.wast:1497
assert_trap(() => call($31, "test", [29]));

// table_copy.wast:1498
assert_trap(() => call($31, "test", [30]));

// table_copy.wast:1499
assert_trap(() => call($31, "test", [31]));

// table_copy.wast:1500
assert_trap(() => call($31, "test", [32]));

// table_copy.wast:1501
assert_trap(() => call($31, "test", [33]));

// table_copy.wast:1502
assert_trap(() => call($31, "test", [34]));

// table_copy.wast:1503
assert_trap(() => call($31, "test", [35]));

// table_copy.wast:1504
assert_trap(() => call($31, "test", [36]));

// table_copy.wast:1505
assert_trap(() => call($31, "test", [37]));

// table_copy.wast:1506
assert_trap(() => call($31, "test", [38]));

// table_copy.wast:1507
assert_trap(() => call($31, "test", [39]));

// table_copy.wast:1508
assert_trap(() => call($31, "test", [40]));

// table_copy.wast:1509
assert_trap(() => call($31, "test", [41]));

// table_copy.wast:1510
assert_trap(() => call($31, "test", [42]));

// table_copy.wast:1511
assert_trap(() => call($31, "test", [43]));

// table_copy.wast:1512
assert_trap(() => call($31, "test", [44]));

// table_copy.wast:1513
assert_trap(() => call($31, "test", [45]));

// table_copy.wast:1514
assert_trap(() => call($31, "test", [46]));

// table_copy.wast:1515
assert_trap(() => call($31, "test", [47]));

// table_copy.wast:1516
assert_trap(() => call($31, "test", [48]));

// table_copy.wast:1517
assert_trap(() => call($31, "test", [49]));

// table_copy.wast:1518
assert_trap(() => call($31, "test", [50]));

// table_copy.wast:1519
assert_trap(() => call($31, "test", [51]));

// table_copy.wast:1520
assert_trap(() => call($31, "test", [52]));

// table_copy.wast:1521
assert_trap(() => call($31, "test", [53]));

// table_copy.wast:1522
assert_trap(() => call($31, "test", [54]));

// table_copy.wast:1523
assert_trap(() => call($31, "test", [55]));

// table_copy.wast:1524
assert_trap(() => call($31, "test", [56]));

// table_copy.wast:1525
assert_trap(() => call($31, "test", [57]));

// table_copy.wast:1526
assert_trap(() => call($31, "test", [58]));

// table_copy.wast:1527
assert_trap(() => call($31, "test", [59]));

// table_copy.wast:1528
assert_trap(() => call($31, "test", [60]));

// table_copy.wast:1529
assert_trap(() => call($31, "test", [61]));

// table_copy.wast:1530
assert_trap(() => call($31, "test", [62]));

// table_copy.wast:1531
assert_trap(() => call($31, "test", [63]));

// table_copy.wast:1532
assert_trap(() => call($31, "test", [64]));

// table_copy.wast:1533
assert_trap(() => call($31, "test", [65]));

// table_copy.wast:1534
assert_trap(() => call($31, "test", [66]));

// table_copy.wast:1535
assert_trap(() => call($31, "test", [67]));

// table_copy.wast:1536
assert_trap(() => call($31, "test", [68]));

// table_copy.wast:1537
assert_trap(() => call($31, "test", [69]));

// table_copy.wast:1538
assert_trap(() => call($31, "test", [70]));

// table_copy.wast:1539
assert_trap(() => call($31, "test", [71]));

// table_copy.wast:1540
assert_trap(() => call($31, "test", [72]));

// table_copy.wast:1541
assert_trap(() => call($31, "test", [73]));

// table_copy.wast:1542
assert_trap(() => call($31, "test", [74]));

// table_copy.wast:1543
assert_trap(() => call($31, "test", [75]));

// table_copy.wast:1544
assert_trap(() => call($31, "test", [76]));

// table_copy.wast:1545
assert_trap(() => call($31, "test", [77]));

// table_copy.wast:1546
assert_trap(() => call($31, "test", [78]));

// table_copy.wast:1547
assert_trap(() => call($31, "test", [79]));

// table_copy.wast:1548
assert_trap(() => call($31, "test", [80]));

// table_copy.wast:1549
assert_trap(() => call($31, "test", [81]));

// table_copy.wast:1550
assert_trap(() => call($31, "test", [82]));

// table_copy.wast:1551
assert_trap(() => call($31, "test", [83]));

// table_copy.wast:1552
assert_trap(() => call($31, "test", [84]));

// table_copy.wast:1553
assert_trap(() => call($31, "test", [85]));

// table_copy.wast:1554
assert_trap(() => call($31, "test", [86]));

// table_copy.wast:1555
assert_trap(() => call($31, "test", [87]));

// table_copy.wast:1556
assert_trap(() => call($31, "test", [88]));

// table_copy.wast:1557
assert_trap(() => call($31, "test", [89]));

// table_copy.wast:1558
assert_trap(() => call($31, "test", [90]));

// table_copy.wast:1559
assert_trap(() => call($31, "test", [91]));

// table_copy.wast:1560
assert_trap(() => call($31, "test", [92]));

// table_copy.wast:1561
assert_trap(() => call($31, "test", [93]));

// table_copy.wast:1562
assert_trap(() => call($31, "test", [94]));

// table_copy.wast:1563
assert_trap(() => call($31, "test", [95]));

// table_copy.wast:1564
assert_trap(() => call($31, "test", [96]));

// table_copy.wast:1565
assert_trap(() => call($31, "test", [97]));

// table_copy.wast:1566
assert_trap(() => call($31, "test", [98]));

// table_copy.wast:1567
assert_trap(() => call($31, "test", [99]));

// table_copy.wast:1568
assert_trap(() => call($31, "test", [100]));

// table_copy.wast:1569
assert_trap(() => call($31, "test", [101]));

// table_copy.wast:1570
assert_trap(() => call($31, "test", [102]));

// table_copy.wast:1571
assert_trap(() => call($31, "test", [103]));

// table_copy.wast:1572
assert_trap(() => call($31, "test", [104]));

// table_copy.wast:1573
assert_trap(() => call($31, "test", [105]));

// table_copy.wast:1574
assert_trap(() => call($31, "test", [106]));

// table_copy.wast:1575
assert_trap(() => call($31, "test", [107]));

// table_copy.wast:1576
assert_trap(() => call($31, "test", [108]));

// table_copy.wast:1577
assert_trap(() => call($31, "test", [109]));

// table_copy.wast:1578
assert_trap(() => call($31, "test", [110]));

// table_copy.wast:1579
assert_trap(() => call($31, "test", [111]));

// table_copy.wast:1580
assert_trap(() => call($31, "test", [112]));

// table_copy.wast:1581
assert_trap(() => call($31, "test", [113]));

// table_copy.wast:1582
assert_trap(() => call($31, "test", [114]));

// table_copy.wast:1583
assert_trap(() => call($31, "test", [115]));

// table_copy.wast:1584
assert_trap(() => call($31, "test", [116]));

// table_copy.wast:1585
assert_trap(() => call($31, "test", [117]));

// table_copy.wast:1586
assert_trap(() => call($31, "test", [118]));

// table_copy.wast:1587
assert_trap(() => call($31, "test", [119]));

// table_copy.wast:1588
assert_trap(() => call($31, "test", [120]));

// table_copy.wast:1589
assert_trap(() => call($31, "test", [121]));

// table_copy.wast:1590
assert_trap(() => call($31, "test", [122]));

// table_copy.wast:1591
assert_trap(() => call($31, "test", [123]));

// table_copy.wast:1592
assert_trap(() => call($31, "test", [124]));

// table_copy.wast:1593
assert_trap(() => call($31, "test", [125]));

// table_copy.wast:1594
assert_trap(() => call($31, "test", [126]));

// table_copy.wast:1595
assert_trap(() => call($31, "test", [127]));
