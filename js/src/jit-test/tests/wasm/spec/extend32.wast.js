// extend32.wast:3
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x00\x07\x9a\x80\x80\x80\x00\x02\x09\x65\x78\x74\x65\x6e\x64\x38\x5f\x73\x00\x00\x0a\x65\x78\x74\x65\x6e\x64\x31\x36\x5f\x73\x00\x01\x0a\x95\x80\x80\x80\x00\x02\x85\x80\x80\x80\x00\x00\x20\x00\xc0\x0b\x85\x80\x80\x80\x00\x00\x20\x00\xc1\x0b");

// extend32.wast:8
assert_return(() => call($1, "extend8_s", [0]), 0);

// extend32.wast:9
assert_return(() => call($1, "extend8_s", [127]), 127);

// extend32.wast:10
assert_return(() => call($1, "extend8_s", [128]), -128);

// extend32.wast:11
assert_return(() => call($1, "extend8_s", [255]), -1);

// extend32.wast:12
assert_return(() => call($1, "extend8_s", [19088640]), 0);

// extend32.wast:13
assert_return(() => call($1, "extend8_s", [-19088768]), -128);

// extend32.wast:14
assert_return(() => call($1, "extend8_s", [-1]), -1);

// extend32.wast:16
assert_return(() => call($1, "extend16_s", [0]), 0);

// extend32.wast:17
assert_return(() => call($1, "extend16_s", [32767]), 32767);

// extend32.wast:18
assert_return(() => call($1, "extend16_s", [32768]), -32768);

// extend32.wast:19
assert_return(() => call($1, "extend16_s", [65535]), -1);

// extend32.wast:20
assert_return(() => call($1, "extend16_s", [19070976]), 0);

// extend32.wast:21
assert_return(() => call($1, "extend16_s", [-19103744]), -32768);

// extend32.wast:22
assert_return(() => call($1, "extend16_s", [-1]), -1);
