/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var MMS = {};
subscriptLoader.loadSubScript("resource://gre/modules/MmsPduHelper.jsm", MMS);
MMS.debug = do_print;

function run_test() {
  run_next_test();
}

//
// Test target: BooleanValue
//

//// BooleanValue.decode ////

add_test(function test_BooleanValue_decode() {
  // Valid codes are 128 and 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_decode_test(MMS.BooleanValue, [0], null, "CodeError");
  wsp_decode_test(MMS.BooleanValue, [1], null, "CodeError");
  wsp_decode_test(MMS.BooleanValue, [127], null, "CodeError");
  wsp_decode_test(MMS.BooleanValue, [128], true);
  wsp_decode_test(MMS.BooleanValue, [129], false);
  wsp_decode_test(MMS.BooleanValue, [130], null, "CodeError");
  wsp_decode_test(MMS.BooleanValue, [255], null, "CodeError");

  run_next_test();
});

//// BooleanValue.encode ////

add_test(function test_BooleanValue_encode() {
  wsp_encode_test(MMS.BooleanValue, true, [128]);
  wsp_encode_test(MMS.BooleanValue, false, [129]);

  run_next_test();
});

//
// Test target: Address
//

//// Address.decode ////

add_test(function test_Address_decode() {
  // Test for PLMN address
  wsp_decode_test(MMS.Address, strToCharCodeArray("+123.456-789/TYPE=PLMN"),
                      {address: "+123.456-789", type: "PLMN"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("123456789/TYPE=PLMN"),
                      {address: "123456789", type: "PLMN"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("a23456789/TYPE=PLMN"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("++123456789/TYPE=PLMN"),
                      null, "CodeError");

  // Test for IPv4
  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3.4/TYPE=IPv4"),
                      {address: "1.2.3.4", type: "IPv4"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3.256/TYPE=IPv4"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3.00/TYPE=IPv4"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3.a/TYPE=IPv4"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3/TYPE=IPv4"),
                      null, "CodeError");

  // Test for IPv6
  wsp_decode_test(MMS.Address,
    strToCharCodeArray("1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000/TYPE=IPv6"),
    {address: "1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000", type: "IPv6"}
  );
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5:6:7:8/TYPE=IPv6"),
                      {address: "1:2:3:4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5:6:7::/TYPE=IPv6"),
                      {address: "1:2:3:4:5:6:7::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5:6::/TYPE=IPv6"),
                      {address: "1:2:3:4:5:6::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5::/TYPE=IPv6"),
                      {address: "1:2:3:4:5::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4::/TYPE=IPv6"),
                      {address: "1:2:3:4::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3::/TYPE=IPv6"),
                      {address: "1:2:3::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::/TYPE=IPv6"),
                      {address: "1:2::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::/TYPE=IPv6"),
                      {address: "1::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::/TYPE=IPv6"),
                      {address: "::", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5:6::8/TYPE=IPv6"),
                      {address: "1:2:3:4:5:6::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5::8/TYPE=IPv6"),
                      {address: "1:2:3:4:5::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4::8/TYPE=IPv6"),
                      {address: "1:2:3:4::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3::8/TYPE=IPv6"),
                      {address: "1:2:3::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::8/TYPE=IPv6"),
                      {address: "1:2::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::8/TYPE=IPv6"),
                      {address: "1::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::8/TYPE=IPv6"),
                      {address: "::8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4:5::7:8/TYPE=IPv6"),
                      {address: "1:2:3:4:5::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4::7:8/TYPE=IPv6"),
                      {address: "1:2:3:4::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3::7:8/TYPE=IPv6"),
                      {address: "1:2:3::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::7:8/TYPE=IPv6"),
                      {address: "1:2::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::7:8/TYPE=IPv6"),
                      {address: "1::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::7:8/TYPE=IPv6"),
                      {address: "::7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3:4::6:7:8/TYPE=IPv6"),
                      {address: "1:2:3:4::6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3::6:7:8/TYPE=IPv6"),
                      {address: "1:2:3::6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::6:7:8/TYPE=IPv6"),
                      {address: "1:2::6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::6:7:8/TYPE=IPv6"),
                      {address: "1::6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::6:7:8/TYPE=IPv6"),
                      {address: "::6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2:3::5:6:7:8/TYPE=IPv6"),
                      {address: "1:2:3::5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::5:6:7:8/TYPE=IPv6"),
                      {address: "1:2::5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::5:6:7:8/TYPE=IPv6"),
                      {address: "1::5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::5:6:7:8/TYPE=IPv6"),
                      {address: "::5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1:2::4:5:6:7:8/TYPE=IPv6"),
                      {address: "1:2::4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::4:5:6:7:8/TYPE=IPv6"),
                      {address: "1::4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::4:5:6:7:8/TYPE=IPv6"),
                      {address: "::4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::3:4:5:6:7:8/TYPE=IPv6"),
                      {address: "1::3:4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::3:4:5:6:7:8/TYPE=IPv6"),
                      {address: "::3:4:5:6:7:8", type: "IPv6"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("::2:3:4:5:6:7:8/TYPE=IPv6"),
                      {address: "::2:3:4:5:6:7:8", type: "IPv6"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("1:g:3:4:5:6:7:8/TYPE=IPv6"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("1::3:4::6:7:8/TYPE=IPv6"),
                      null, "CodeError");

  // Test for other device-address
  wsp_decode_test(MMS.Address, strToCharCodeArray("+H-e.1%l_o/TYPE=W0r1d_"),
                      {address: "+H-e.1%l_o", type: "W0r1d_"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("addr/TYPE=type!"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("addr!/TYPE=type"),
                      null, "CodeError");

  // Test for num-shortcode
  wsp_decode_test(MMS.Address, strToCharCodeArray("1"),
                      {address: "1", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("123"),
                      {address: "123", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("+123"),
                      {address: "+123", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("*123"),
                      {address: "*123", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("#123"),
                      {address: "#123", type: "num"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("++123"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("!123"),
                      null, "CodeError");
  wsp_decode_test(MMS.Address, strToCharCodeArray("1*23"),
                      null, "CodeError");

  // Test for alphanum-shortcode
  wsp_decode_test(MMS.Address, strToCharCodeArray("a"),
                      {address: "a", type: "alphanum"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("H0w_D0_Y0u_Turn_Th1s_0n"),
                      {address: "H0w_D0_Y0u_Turn_Th1s_0n", type: "alphanum"});

  wsp_decode_test(MMS.Address, strToCharCodeArray("abc#"),
                      null, "CodeError");

  // Test for email address
  wsp_decode_test(MMS.Address, strToCharCodeArray("Joe User <joe@user.org>"),
                      {address: "Joe User <joe@user.org>", type: "email"});
  wsp_decode_test(MMS.Address,
    strToCharCodeArray("a-z.A-.Z.0-9!#$.%&.'*+./=?^._`{|}~-@a-.zA-Z.0-9!.#$%&'.*+/=?.^_`.{|}~-"),
    {address: "a-z.A-.Z.0-9!#$.%&.'*+./=?^._`{|}~-@a-.zA-Z.0-9!.#$%&'.*+/=?.^_`.{|}~-", type: "email"}
  );

  // Test for invalid address
  wsp_decode_test(MMS.Address, strToCharCodeArray("@@@@@"),
                  null, "CodeError");

  run_next_test();
});

//// Address.encode ////

add_test(function test_Address_encode() {
  // Test for PLMN address
  wsp_encode_test(MMS.Address, {address: "+123.456-789", type: "PLMN"},
                  strToCharCodeArray("+123.456-789/TYPE=PLMN"));
  wsp_encode_test(MMS.Address, {address: "123456789", type: "PLMN"},
                  strToCharCodeArray("123456789/TYPE=PLMN"));

  wsp_encode_test(MMS.Address, {address: "a23456789", type: "PLMN"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "++123456789", type: "PLMN"},
                  null, "CodeError");

  // Test for IPv4
  wsp_encode_test(MMS.Address, {address: "1.2.3.4", type: "IPv4"},
                  strToCharCodeArray("1.2.3.4/TYPE=IPv4"));

  wsp_encode_test(MMS.Address, {address: "1.2.3.256", type: "IPv4"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "1.2.3.00", type: "IPv4"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "1.2.3.a", type: "IPv4"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "1.2.3", type: "IPv4"},
                  null, "CodeError");

  // Test for IPv6
  wsp_encode_test(MMS.Address,
    {address: "1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000", type: "IPv6"},
    strToCharCodeArray("1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000/TYPE=IPv6")
  );
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5:6:7::", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5:6:7::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5:6::", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5:6::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5::", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4::", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3::", type: "IPv6"},
                  strToCharCodeArray("1:2:3::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::", type: "IPv6"},
                  strToCharCodeArray("1:2::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::", type: "IPv6"},
                  strToCharCodeArray("1::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::", type: "IPv6"},
                  strToCharCodeArray("::/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5:6::8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5:6::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5::8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4::8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3::8", type: "IPv6"},
                  strToCharCodeArray("1:2:3::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::8", type: "IPv6"},
                  strToCharCodeArray("1:2::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::8", type: "IPv6"},
                  strToCharCodeArray("1::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::8", type: "IPv6"},
                  strToCharCodeArray("::8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4:5::7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4:5::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4::7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3::7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::7:8", type: "IPv6"},
                  strToCharCodeArray("1:2::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::7:8", type: "IPv6"},
                  strToCharCodeArray("1::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::7:8", type: "IPv6"},
                  strToCharCodeArray("::7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3:4::6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3:4::6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3::6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3::6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2::6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::6:7:8", type: "IPv6"},
                  strToCharCodeArray("1::6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::6:7:8", type: "IPv6"},
                  strToCharCodeArray("::6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2:3::5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2:3::5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2::5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1::5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("::5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1:2::4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1:2::4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1::4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("::4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "1::3:4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("1::3:4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::3:4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("::3:4:5:6:7:8/TYPE=IPv6"));
  wsp_encode_test(MMS.Address, {address: "::2:3:4:5:6:7:8", type: "IPv6"},
                  strToCharCodeArray("::2:3:4:5:6:7:8/TYPE=IPv6"));

  wsp_encode_test(MMS.Address, {address: "1:g:3:4:5:6:7:8", type: "IPv6"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "1::3:4:5:6::8", type: "IPv6"},
                  null, "CodeError");

  // Test for other device-address
  wsp_encode_test(MMS.Address, {address: "+H-e.1%l_o", type: "W0r1d_"},
                  strToCharCodeArray("+H-e.1%l_o/TYPE=W0r1d_"));

  wsp_encode_test(MMS.Address, {address: "addr!", type: "type"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "addr", type: "type!"},
                  null, "CodeError");

  // Test for num-shortcode
  wsp_encode_test(MMS.Address, {address: "1", type: "num"},
                  strToCharCodeArray("1"));
  wsp_encode_test(MMS.Address, {address: "123", type: "num"},
                  strToCharCodeArray("123"));
  wsp_encode_test(MMS.Address, {address: "+123", type: "num"},
                  strToCharCodeArray("+123"));
  wsp_encode_test(MMS.Address, {address: "*123", type: "num"},
                  strToCharCodeArray("*123"));
  wsp_encode_test(MMS.Address, {address: "#123", type: "num"},
                  strToCharCodeArray("#123"));

  wsp_encode_test(MMS.Address, {address: "++123", type: "num"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "!123", type: "num"},
                  null, "CodeError");
  wsp_encode_test(MMS.Address, {address: "1#23", type: "num"},
                  null, "CodeError");

  // Test for alphanum-shortcode
  wsp_encode_test(MMS.Address, {address: "a", type: "alphanum"},
                  strToCharCodeArray("a"));
  wsp_encode_test(MMS.Address, {address: "1", type: "alphanum"},
                  strToCharCodeArray("1"));
  wsp_encode_test(MMS.Address, {address: "H0w_D0_Y0u_Turn_Th1s_0n", type: "alphanum"},
                  strToCharCodeArray("H0w_D0_Y0u_Turn_Th1s_0n"));

  wsp_encode_test(MMS.Address, {address: "abc#", type: "alphanum"},
                  null, "CodeError");

  // Test for email address
  wsp_encode_test(MMS.Address, {address: "Joe User <joe@user.org>", type: "email"},
                  strToCharCodeArray("Joe User <joe@user.org>"));
  wsp_encode_test(MMS.Address,
    {address: "a-z.A-.Z.0-9!#$.%&.'*+./=?^._`{|}~-@a-.zA-Z.0-9!.#$%&'.*+/=?.^_`.{|}~-", type: "email"},
    strToCharCodeArray("a-z.A-.Z.0-9!#$.%&.'*+./=?^._`{|}~-@a-.zA-Z.0-9!.#$%&'.*+/=?.^_`.{|}~-")
  );

  // Test for invalid address
  wsp_encode_test(MMS.Address, {address: "a"}, null, "CodeError");
  wsp_encode_test(MMS.Address, {type: "alphanum"}, null, "CodeError");

  run_next_test();
});

//// Address.resolveType ////

add_test(function test_Address_encode() {
  function test(address, type) {
    do_check_eq(MMS.Address.resolveType(address), type);
  }

  // Test ambiguous addresses.
  test("+15525225554",      "PLMN");
  test("5525225554",        "PLMN");
  test("jkalbcjklg",        "PLMN");
  test("jk.alb.cjk.lg",     "PLMN");
  test("j:k:a:l:b:c:jk:lg", "PLMN");
  test("55.252.255.54",     "IPv4");
  test("5:5:2:5:2:2:55:54", "IPv6");
  test("jk@alb.cjk.lg",     "email");
  // Test empty address.  This is for received anonymous MMS messages.
  test("",                  "Others");

  run_next_test();
});

//
// Test target: HeaderField
//

//// HeaderField.decode ////

add_test(function test_HeaderField_decode() {
  wsp_decode_test(MMS.HeaderField, [65, 0, 66, 0], {name: "a", value: "B"});
  wsp_decode_test(MMS.HeaderField, [0x80 | 0x27, 128],
                      {name: "x-mms-stored", value: true});

  run_next_test();
});

//// HeaderField.encode ////

add_test(function test_HeaderField_encode() {
  // Test for MmsHeader
  wsp_encode_test(MMS.HeaderField, {name: "X-Mms-Message-Type",
                                    value: MMS_PDU_TYPE_SEND_REQ},
                  [0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ]);
  // Test for ApplicationHeader
  wsp_encode_test(MMS.HeaderField, {name: "a", value: "B"}, [97, 0, 66, 0]);

  run_next_test();
});

//
// Test target: MmsHeader
//

//// MmsHeader.decode ////

add_test(function test_MmsHeader_decode() {
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x00], null, "NotWellKnownEncodingError");
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x27, 128],
                      {name: "x-mms-stored", value: true});
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x27, 255], null);

  run_next_test();
});

//// MmsHeader.encode ////

add_test(function test_MmsHeader_encode() {
  // Test for empty header name:
  wsp_encode_test(MMS.MmsHeader, {name: undefined, value: null}, null, "CodeError");
  wsp_encode_test(MMS.MmsHeader, {name: null, value: null}, null, "CodeError");
  wsp_encode_test(MMS.MmsHeader, {name: "", value: null}, null, "CodeError");
  // Test for non-well-known header name:
  wsp_encode_test(MMS.MmsHeader, {name: "X-No-Such-Field", value: null},
                  null, "NotWellKnownEncodingError");
  // Test for normal header
  wsp_encode_test(MMS.MmsHeader, {name: "X-Mms-Message-Type",
                                  value: MMS_PDU_TYPE_SEND_REQ},
                  [0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ]);

  run_next_test();
});

//
// Test target: CancelStatusValue
//

//// CancelStatusValue.decode ////

add_test(function test_CancelStatusValue_decode() {
  // Valid codes are 128 and 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_decode_test(MMS.CancelStatusValue, [0], null, "CodeError");
  wsp_decode_test(MMS.CancelStatusValue, [1], null, "CodeError");
  wsp_decode_test(MMS.CancelStatusValue, [127], null, "CodeError");
  wsp_decode_test(MMS.CancelStatusValue, [128], 128);
  wsp_decode_test(MMS.CancelStatusValue, [129], 129);
  wsp_decode_test(MMS.CancelStatusValue, [130], null, "CodeError");
  wsp_decode_test(MMS.CancelStatusValue, [255], null, "CodeError");

  run_next_test();
});

//// CancelStatusValue.encode ////

add_test(function test_CancelStatusValue_encode() {
  // Valid codes are 128 and 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_encode_test(MMS.CancelStatusValue, 0, null, "CodeError");
  wsp_encode_test(MMS.CancelStatusValue, 1, null, "CodeError");
  wsp_encode_test(MMS.CancelStatusValue, 127, null, "CodeError");
  wsp_encode_test(MMS.CancelStatusValue, 128, [128]);
  wsp_encode_test(MMS.CancelStatusValue, 129, [129]);
  wsp_encode_test(MMS.CancelStatusValue, 130, null, "CodeError");
  wsp_encode_test(MMS.CancelStatusValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: ContentClassValue
//

//// ContentClassValue.decode ////

add_test(function test_ContentClassValue_decode() {
  // Valid codes are 128 - 135. Check boundary conditions 0, 1, 127, 136 and
  // 255 as well.
  wsp_decode_test(MMS.ContentClassValue, [0], null, "CodeError");
  wsp_decode_test(MMS.ContentClassValue, [1], null, "CodeError");
  wsp_decode_test(MMS.ContentClassValue, [127], null, "CodeError");
  for (let i = 128; i <= 135; i++) {
    wsp_decode_test(MMS.ContentClassValue, [i], i);
  }
  wsp_decode_test(MMS.ContentClassValue, [136], null, "CodeError");
  wsp_decode_test(MMS.ContentClassValue, [255], null, "CodeError");

  run_next_test();
});

//// ContentClassValue.encode ////

add_test(function test_ContentClassValue_encode() {
  // Valid codes are 128 - 135. Check boundary conditions 0, 1, 127, 136 and
  // 255 as well.
  wsp_encode_test(MMS.ContentClassValue, 0, null, "CodeError");
  wsp_encode_test(MMS.ContentClassValue, 1, null, "CodeError");
  wsp_encode_test(MMS.ContentClassValue, 127, null, "CodeError");
  for (let i = 128; i <= 135; i++) {
    wsp_encode_test(MMS.ContentClassValue, i, [i]);
  }
  wsp_encode_test(MMS.ContentClassValue, 136, null, "CodeError");
  wsp_encode_test(MMS.ContentClassValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: ContentLocationValue
//

//// ContentLocationValue.decode ////

add_test(function test_ContentLocationValue_decode() {
  // Test for MMS_PDU_TYPE_MBOX_DELETE_CONF & MMS_PDU_TYPE_DELETE_CONF
  function test(type, statusCount, exception) {
    function decode(data) {
      let options = {};
      if (type) {
        options["x-mms-message-type"] = type;
      }
      return MMS.ContentLocationValue.decode(data, options);
    }

    let uri = "http://no.such.com/path";

    let data = strToCharCodeArray(uri);
    if (statusCount != null) {
      data = [data.length + 1, statusCount | 0x80].concat(data);
    }

    let expected;
    if (!exception) {
      expected = {};
      if (statusCount != null) {
        expected.statusCount = statusCount;
      }
      expected.uri = uri;
    }

    do_print("data = " + JSON.stringify(data));
    wsp_decode_test_ex(decode, data, expected, exception);
  }

  test(null, null, "FatalCodeError");
  for (let type = MMS_PDU_TYPE_SEND_REQ; type <= MMS_PDU_TYPE_CANCEL_CONF; type++) {
    if ((type == MMS_PDU_TYPE_MBOX_DELETE_CONF)
        || (type == MMS_PDU_TYPE_DELETE_CONF)) {
      test(type, 1, null);
    } else {
      test(type, null, null);
    }
  }

  run_next_test();
});

//
// Test target: ElementDescriptorValue
//

//// ElementDescriptorValue.decode ////

add_test(function test_ElementDescriptorValue_decode() {
  wsp_decode_test(MMS.ElementDescriptorValue, [2, 97, 0], {contentReference: "a"});
  wsp_decode_test(MMS.ElementDescriptorValue, [4, 97, 0, 0x80 | 0x02, 0x80],
                      {contentReference: "a", params: {type: 0}});

  run_next_test();
});

//
// Test target: Parameter
//

//// Parameter.decodeParameterName ////

add_test(function test_Parameter_decodeParameterName() {
  wsp_decode_test_ex(function(data) {
      return MMS.Parameter.decodeParameterName(data);
    }, [0x80 | 0x02], "type"
  );
  wsp_decode_test_ex(function(data) {
      return MMS.Parameter.decodeParameterName(data);
    }, strToCharCodeArray("type"), "type"
  );

  run_next_test();
});

//// Parameter.decode ////

add_test(function test_Parameter_decode() {
  wsp_decode_test(MMS.Parameter, [0x80 | 0x02, 0x80 | 0x00], {name: "type", value: 0});

  run_next_test();
});

//// Parameter.decodeMultiple ////

add_test(function test_Parameter_decodeMultiple() {
  // FIXME: The following test case falls because Parameter-value decoding of
  //        "type" parameters utilies WSP.ConstrainedEncoding, which in turn
  //        utilies WSP.TextString, and TextString is not matual exclusive to
  //        each other.
  //wsp_decode_test_ex(function(data) {
  //    return MMS.Parameter.decodeMultiple(data, data.array.length);
  //  }, [0x80 | 0x02, 0x80 | 0x00].concat(strToCharCodeArray("good")).concat([0x80 | 0x01]),
  //  {type: 0, good: 1}
  //);

  run_next_test();
});

//// Parameter.encode ////

add_test(function test_Parameter_encode() {
  // Test for invalid parameter value
  wsp_encode_test(MMS.Parameter, null, null, "CodeError");
  wsp_encode_test(MMS.Parameter, undefined, null, "CodeError");
  wsp_encode_test(MMS.Parameter, {}, null, "CodeError");
  // Test for case-insensitive parameter name
  wsp_encode_test(MMS.Parameter, {name: "TYPE", value: 0}, [130, 128]);
  wsp_encode_test(MMS.Parameter, {name: "type", value: 0}, [130, 128]);
  // Test for non-well-known parameter name
  wsp_encode_test(MMS.Parameter, {name: "name", value: 0}, [110, 97, 109, 101, 0, 128]);
  // Test for constrained encoding value
  wsp_encode_test(MMS.Parameter, {name: "type", value: "0"}, [130, 48, 0]);

  run_next_test();
});

//
// Test target: EncodedStringValue
//

//// EncodedStringValue.decode ////

add_test(function test_EncodedStringValue_decode() {
  // Test for normal TextString
  wsp_decode_test(MMS.EncodedStringValue, strToCharCodeArray("Hello"), "Hello");
  // Test for non-well-known charset
  wsp_decode_test(MMS.EncodedStringValue, [1, 0x80], null, "NotWellKnownEncodingError");
  // Test for utf-8
  let entry = MMS.WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"];
  // "Mozilla" in full width.
  let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
             .createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = entry.converter;

  let raw = conv.convertToByteArray(str).concat([0]);
  wsp_decode_test(MMS.EncodedStringValue,
                  [raw.length + 2, 0x80 | entry.number, 127].concat(raw), str);

  entry = MMS.WSP.WSP_WELL_KNOWN_CHARSETS["utf-16"];
  // "Mozilla" in full width.
  str = "\u004d\u006F\u007A\u0069\u006C\u006C\u0061";

  conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
         .createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = entry.converter;

  raw = conv.convertToByteArray(str).concat([0]);
  wsp_decode_test(MMS.EncodedStringValue,
                  [raw.length + 3, 2, 3, 247].concat(raw), str);

  run_next_test();
});

//// EncodedStringValue.encode ////

add_test(function test_EncodedStringValue_encode() {
  // Test for normal TextString
  wsp_encode_test(MMS.EncodedStringValue, "Hello", strToCharCodeArray("Hello"));

  // Test for utf-8
  let entry = MMS.WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"];
  // "Mozilla" in full width.
  let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
             .createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = entry.converter;

  let raw = conv.convertToByteArray(str).concat([0]);
  wsp_encode_test(MMS.EncodedStringValue, str,
                  [raw.length + 2, 0x80 | entry.number, 127].concat(raw));

  // MMS.EncodedStringValue encodes non us-ascii characters (128 ~ 255)
  // (e.g., 'Ñ' or 'ü') by the utf-8 encoding. Otherwise, for us-ascii
  // characters (0 ~ 127), still use the normal TextString encoding.

  // "Ñü" in full width.
  str = "\u00d1\u00fc";
  raw = conv.convertToByteArray(str).concat([0]);
  wsp_encode_test(MMS.EncodedStringValue, str,
                  [raw.length + 2, 0x80 | entry.number, 127].concat(raw));

  run_next_test();
});

//
// Test target: ExpiryValue
//

//// ExpiryValue.decode ////

add_test(function test_ExpiryValue_decode() {
  // Test for Absolute-token Date-value
  wsp_decode_test(MMS.ExpiryValue, [3, 128, 1, 0x80], new Date(0x80 * 1000));
  // Test for Relative-token Delta-seconds-value
  wsp_decode_test(MMS.ExpiryValue, [2, 129, 0x80], 0);

  run_next_test();
});

//// ExpiryValue.encode ////

add_test(function test_ExpiryValue_encode() {
  // Test for Absolute-token Date-value
  wsp_encode_test(MMS.ExpiryValue, new Date(0x80 * 1000), [3, 128, 1, 0x80]);
  // Test for Relative-token Delta-seconds-value
  wsp_encode_test(MMS.ExpiryValue, 0, [2, 129, 0x80]);

  run_next_test();
});

//
// Test target: PreviouslySentByValue
//

//// PreviouslySentByValue.decode ////

add_test(function test_PreviouslySentByValue_decode() {
  wsp_decode_test(MMS.PreviouslySentByValue, [3, 0x80 | 0x03, 65, 0],
                      {forwardedCount: 3, originator: {address: "A",
                                                       type: "alphanum"}});

  run_next_test();
});

//
// Test target: PreviouslySentDateValue
//

//// PreviouslySentDateValue.decode ////

add_test(function test_PreviouslySentDateValue_decode() {
  wsp_decode_test(MMS.PreviouslySentDateValue, [3, 0x80 | 0x03, 1, 4],
                      {forwardedCount: 3, timestamp: new Date(4 * 1000)});

  run_next_test();
});

//
// Test target: FromValue
//

//// FromValue.decode ////

add_test(function test_FromValue_decode() {
  // Test for Insert-address-token:
  wsp_decode_test(MMS.FromValue, [1, 129], null);
  // Test for Address-present-token:
  let addr = strToCharCodeArray("+123/TYPE=PLMN");
  wsp_decode_test(MMS.FromValue, [addr.length + 1, 128].concat(addr),
                      {address: "+123", type: "PLMN"});

  run_next_test();
});

//// FromValue.encode ////

add_test(function test_FromValue_encode() {
  // Test for Insert-address-token:
  wsp_encode_test(MMS.FromValue, null, [1, 129]);
  // Test for Address-present-token:
  let addr = strToCharCodeArray("+123/TYPE=PLMN");
  wsp_encode_test(MMS.FromValue, {address: "+123", type: "PLMN"},
                  [addr.length + 1, 128].concat(addr));

  run_next_test();
});

//
// Test target: MessageClassValue
//

//// MessageClassValue.decodeClassIdentifier ////

add_test(function test_MessageClassValue_decodeClassIdentifier() {
  const IDs = ["personal", "advertisement", "informational", "auto"];

  function test(i, error) {
    let id = IDs[i - 128];
    wsp_decode_test_ex(function(data) {
        return MMS.MessageClassValue.decodeClassIdentifier(data);
      }, [i], (error ? null : id), error);
  }

  // Valid codes are 128 - 131. Check boundary conditions 0, 1, 127, 132 and
  // 255 as well.
  test(0, "CodeError");
  test(1, "CodeError");
  test(127, "CodeError");
  for (let i = 128; i <= 131; i++) {
    test(i, null);
  }
  test(132, "CodeError");
  test(255, "CodeError");

  run_next_test();
});

//// MessageClassValue.decode ////

add_test(function test_MessageClassValue_decode() {
  wsp_decode_test(MMS.MessageClassValue, [65, 0], "A");
  wsp_decode_test(MMS.MessageClassValue, [128], "personal");

  run_next_test();
});

//// MessageClassValue.encode ////

add_test(function test_MessageClassValue_encode() {
  wsp_encode_test(MMS.MessageClassValue, "personal", [128]);
  wsp_encode_test(MMS.MessageClassValue, "advertisement", [129]);
  wsp_encode_test(MMS.MessageClassValue, "informational", [130]);
  wsp_encode_test(MMS.MessageClassValue, "auto", [131]);
  wsp_encode_test(MMS.MessageClassValue, "A", [65, 0]);

  run_next_test();
});

//
// Test target: MessageTypeValue
//

//// MessageTypeValue.decode ////

add_test(function test_MessageTypeValue_decode() {
  // Valid codes are 128 - 151. Check boundary conditions 0, 1, 127, 152 and
  // 255 as well.
  wsp_decode_test(MMS.MessageTypeValue, [0], null, "CodeError");
  wsp_decode_test(MMS.MessageTypeValue, [1], null, "CodeError");
  wsp_decode_test(MMS.MessageTypeValue, [127], null, "CodeError");
  for (let i = 128; i <= 151; i++) {
    wsp_decode_test(MMS.MessageTypeValue, [i], i);
  }
  wsp_decode_test(MMS.MessageTypeValue, [152], null, "CodeError");
  wsp_decode_test(MMS.MessageTypeValue, [255], null, "CodeError");

  run_next_test();
});

//// MessageTypeValue.encode ////

add_test(function test_MessageTypeValue_encode() {
  // Valid codes are 128 - 151. Check boundary conditions 0, 1, 127, 152 and
  // 255 as well.
  wsp_encode_test(MMS.MessageTypeValue, 0, null, "CodeError");
  wsp_encode_test(MMS.MessageTypeValue, 1, null, "CodeError");
  wsp_encode_test(MMS.MessageTypeValue, 127, null, "CodeError");
  for (let i = 128; i <= 151; i++) {
    wsp_encode_test(MMS.MessageTypeValue, i, [i]);
  }
  wsp_encode_test(MMS.MessageTypeValue, 152, null, "CodeError");
  wsp_encode_test(MMS.MessageTypeValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: MmFlagsValue
//

//// MmFlagsValue.decode ////

add_test(function test_MmFlagsValue_decode() {
  // Valid codes are 128 - 130. Check boundary conditions 0, 1, 127, 131 and
  // 255 as well.
  wsp_decode_test(MMS.MmFlagsValue, [3, 0, 65, 0], null, "CodeError");
  wsp_decode_test(MMS.MmFlagsValue, [3, 1, 65, 0], null, "CodeError");
  wsp_decode_test(MMS.MmFlagsValue, [3, 127, 65, 0], null, "CodeError");
  for (let i = 128; i <= 130; i++) {
    wsp_decode_test(MMS.MmFlagsValue, [3, i, 65, 0], {type: i, text: "A"});
  }
  wsp_decode_test(MMS.MmFlagsValue, [3, 131, 65, 0], null, "CodeError");
  wsp_decode_test(MMS.MmFlagsValue, [3, 255, 65, 0], null, "CodeError");

  run_next_test();
});

//// MmFlagsValue.encode ////

add_test(function test_MmFlagsValue_encode() {
  // Valid codes are 128 - 130. Check boundary conditions 0, 1, 127, 131 and
  // 255 as well.
  wsp_encode_test(MMS.MmFlagsValue, {type: 0, text: "A"}, null, "CodeError");
  wsp_encode_test(MMS.MmFlagsValue, {type: 1, text: "A"}, null, "CodeError");
  wsp_encode_test(MMS.MmFlagsValue, {type: 127, text: "A"}, null, "CodeError");
  for (let i = 128; i <= 130; i++) {
    wsp_encode_test(MMS.MmFlagsValue, {type: i, text: "A"}, [3, i, 65, 0]);
  }
  wsp_encode_test(MMS.MmFlagsValue, {type: 131, text: "A"}, null, "CodeError");
  wsp_encode_test(MMS.MmFlagsValue, {type: 255, text: "A"}, null, "CodeError");

  run_next_test();
});

//
// Test target: MmStateValue
//

//// MmStateValue.decode ////

add_test(function test_MmStateValue_decode() {
  // Valid codes are 128 - 132. Check boundary conditions 0, 1, 127, 133 and
  // 255 as well.
  wsp_decode_test(MMS.MmStateValue, [0], null, "CodeError");
  wsp_decode_test(MMS.MmStateValue, [1], null, "CodeError");
  wsp_decode_test(MMS.MmStateValue, [127], null, "CodeError");
  for (let i = 128; i <= 132; i++) {
    wsp_decode_test(MMS.MmStateValue, [i], i);
  }
  wsp_decode_test(MMS.MmStateValue, [133], null, "CodeError");
  wsp_decode_test(MMS.MmStateValue, [255], null, "CodeError");

  run_next_test();
});

//// MmStateValue.encode ////

add_test(function test_MmStateValue_encode() {
  // Valid codes are 128 - 132. Check boundary conditions 0, 1, 127, 133 and
  // 255 as well.
  wsp_encode_test(MMS.MmStateValue, 0, null, "CodeError");
  wsp_encode_test(MMS.MmStateValue, 1, null, "CodeError");
  wsp_encode_test(MMS.MmStateValue, 127, null, "CodeError");
  for (let i = 128; i <= 132; i++) {
    wsp_encode_test(MMS.MmStateValue, i, [i]);
  }
  wsp_encode_test(MMS.MmStateValue, 133, null, "CodeError");
  wsp_encode_test(MMS.MmStateValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: PriorityValue
//

//// PriorityValue.decode ////

add_test(function test_PriorityValue_decode() {
  // Valid codes are 128 - 130. Check boundary conditions 0, 1, 127, 131 and
  // 255 as well.
  wsp_decode_test(MMS.PriorityValue, [0], null, "CodeError");
  wsp_decode_test(MMS.PriorityValue, [1], null, "CodeError");
  wsp_decode_test(MMS.PriorityValue, [127], null, "CodeError");
  for (let i = 128; i <= 130; i++) {
    wsp_decode_test(MMS.PriorityValue, [i], i);
  }
  wsp_decode_test(MMS.PriorityValue, [131], null, "CodeError");
  wsp_decode_test(MMS.PriorityValue, [255], null, "CodeError");

  run_next_test();
});

//// PriorityValue.encode ////

add_test(function test_PriorityValue_encode() {
  // Valid codes are 128 - 130. Check boundary conditions 0, 1, 127, 131 and
  // 255 as well.
  wsp_encode_test(MMS.PriorityValue, 0, null, "CodeError");
  wsp_encode_test(MMS.PriorityValue, 1, null, "CodeError");
  wsp_encode_test(MMS.PriorityValue, 127, null, "CodeError");
  for (let i = 128; i <= 130; i++) {
    wsp_encode_test(MMS.PriorityValue, i, [i]);
  }
  wsp_encode_test(MMS.PriorityValue, 131, null, "CodeError");
  wsp_encode_test(MMS.PriorityValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: ReadStatusValue
//

//// ReadStatusValue.decode ////

add_test(function test_ReadStatusValue_decode() {
  // Valid codes are 128, 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_decode_test(MMS.ReadStatusValue, [0], null, "CodeError");
  wsp_decode_test(MMS.ReadStatusValue, [1], null, "CodeError");
  wsp_decode_test(MMS.ReadStatusValue, [127], null, "CodeError");
  wsp_decode_test(MMS.ReadStatusValue, [128], 128);
  wsp_decode_test(MMS.ReadStatusValue, [129], 129);
  wsp_decode_test(MMS.ReadStatusValue, [130], null, "CodeError");
  wsp_decode_test(MMS.ReadStatusValue, [255], null, "CodeError");

  run_next_test();
});

//// ReadStatusValue.encode ////

add_test(function test_ReadStatusValue_encode() {
  // Valid codes are 128, 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_encode_test(MMS.ReadStatusValue, 0, null, "CodeError");
  wsp_encode_test(MMS.ReadStatusValue, 1, null, "CodeError");
  wsp_encode_test(MMS.ReadStatusValue, 127, null, "CodeError");
  wsp_encode_test(MMS.ReadStatusValue, 128, [128]);
  wsp_encode_test(MMS.ReadStatusValue, 129, [129]);
  wsp_encode_test(MMS.ReadStatusValue, 130, null, "CodeError");
  wsp_encode_test(MMS.ReadStatusValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: RecommendedRetrievalModeValue
//

//// RecommendedRetrievalModeValue.decode ////

add_test(function test_RecommendedRetrievalModeValue_decode() {
  // Valid codes is 128. Check boundary conditions 0, 1, 127, 130 and 255 as
  // well.
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [0], null, "CodeError");
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [1], null, "CodeError");
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [127], null, "CodeError");
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [128], 128);
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [129], null, "CodeError");
  wsp_decode_test(MMS.RecommendedRetrievalModeValue, [255], null, "CodeError");

  run_next_test();
});

//
// Test target: ReplyChargingValue
//

//// ReplyChargingValue.decode ////

add_test(function test_ReplyChargingValue_decode() {
  // Valid codes are 128 - 131. Check boundary conditions 0, 1, 127, 132 and
  // 255 as well.
  wsp_decode_test(MMS.ReplyChargingValue, [0], null, "CodeError");
  wsp_decode_test(MMS.ReplyChargingValue, [1], null, "CodeError");
  wsp_decode_test(MMS.ReplyChargingValue, [127], null, "CodeError");
  for (let i = 128; i <= 131; i++) {
    wsp_decode_test(MMS.ReplyChargingValue, [i], i);
  }
  wsp_decode_test(MMS.ReplyChargingValue, [132], null, "CodeError");
  wsp_decode_test(MMS.ReplyChargingValue, [255], null, "CodeError");

  run_next_test();
});

//// ReplyChargingValue.encode ////

add_test(function test_ReplyChargingValue_encode() {
  // Valid codes are 128 - 131. Check boundary conditions 0, 1, 127, 132 and
  // 255 as well.
  wsp_encode_test(MMS.ReplyChargingValue, 0, null, "CodeError");
  wsp_encode_test(MMS.ReplyChargingValue, 1, null, "CodeError");
  wsp_encode_test(MMS.ReplyChargingValue, 127, null, "CodeError");
  for (let i = 128; i <= 131; i++) {
    wsp_encode_test(MMS.ReplyChargingValue, i, [i]);
  }
  wsp_encode_test(MMS.ReplyChargingValue, 132, null, "CodeError");
  wsp_encode_test(MMS.ReplyChargingValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: ResponseText
//

//// ResponseText.decode ////

add_test(function test_ResponseText_decode() {
  // Test for MMS_PDU_TYPE_MBOX_DELETE_CONF & MMS_PDU_TYPE_DELETE_CONF
  wsp_decode_test_ex(function(data) {
      data.array[0] = data.array.length - 1;

      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_MBOX_DELETE_CONF;
      return MMS.ResponseText.decode(data, options);
    }, [0, 0x80 | 0x00].concat(strToCharCodeArray("http://no.such.com/path")),
    {statusCount: 0, text: "http://no.such.com/path"}
  );
  wsp_decode_test_ex(function(data) {
      data.array[0] = data.array.length - 1;

      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_DELETE_CONF;
      return MMS.ResponseText.decode(data, options);
    }, [0, 0x80 | 0x00].concat(strToCharCodeArray("http://no.such.com/path")),
    {statusCount: 0, text: "http://no.such.com/path"}
  );
  // Test for other situations
  wsp_decode_test_ex(function(data) {
      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_SEND_REQ;
      return MMS.ResponseText.decode(data, options);
    }, strToCharCodeArray("http://no.such.com/path"),
    {text: "http://no.such.com/path"}
  );

  run_next_test();
});

//
// Test target: RetrieveStatusValue
//

//// RetrieveStatusValue.decode ////

add_test(function test_RetrieveStatusValue_decode() {
  // Valid codes are 128, 192 - 255. Check boundary conditions 0, 1, 127, 129,
  // and 191 as well.
  wsp_decode_test(MMS.RetrieveStatusValue, [0],
                  MMS_PDU_ERROR_PERMANENT_FAILURE);
  wsp_decode_test(MMS.RetrieveStatusValue, [1],
                  MMS_PDU_ERROR_PERMANENT_FAILURE);
  wsp_decode_test(MMS.RetrieveStatusValue, [127],
                  MMS_PDU_ERROR_PERMANENT_FAILURE);

  wsp_decode_test(MMS.RetrieveStatusValue, [128], MMS_PDU_ERROR_OK);

  wsp_decode_test(MMS.RetrieveStatusValue, [129],
                  MMS_PDU_ERROR_PERMANENT_FAILURE);
  wsp_decode_test(MMS.RetrieveStatusValue, [191],
                  MMS_PDU_ERROR_PERMANENT_FAILURE);
  for (let i = 192; i < 256; i++) {
    wsp_decode_test(MMS.RetrieveStatusValue, [i], i);
  }

  run_next_test();
});

//
// Test target: SenderVisibilityValue
//

//// SenderVisibilityValue.decode ////

add_test(function test_SenderVisibilityValue_decode() {
  // Valid codes are 128, 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_decode_test(MMS.SenderVisibilityValue, [0], null, "CodeError");
  wsp_decode_test(MMS.SenderVisibilityValue, [1], null, "CodeError");
  wsp_decode_test(MMS.SenderVisibilityValue, [127], null, "CodeError");
  wsp_decode_test(MMS.SenderVisibilityValue, [128], 128);
  wsp_decode_test(MMS.SenderVisibilityValue, [129], 129);
  wsp_decode_test(MMS.SenderVisibilityValue, [130], null, "CodeError");
  wsp_decode_test(MMS.SenderVisibilityValue, [255], null, "CodeError");

  run_next_test();
});

//// SenderVisibilityValue.encode ////

add_test(function test_SenderVisibilityValue_encode() {
  // Valid codes are 128, 129. Check boundary conditions 0, 1, 127, 130 and
  // 255 as well.
  wsp_encode_test(MMS.SenderVisibilityValue, 0, null, "CodeError");
  wsp_encode_test(MMS.SenderVisibilityValue, 1, null, "CodeError");
  wsp_encode_test(MMS.SenderVisibilityValue, 127, null, "CodeError");
  wsp_encode_test(MMS.SenderVisibilityValue, 128, [128]);
  wsp_encode_test(MMS.SenderVisibilityValue, 129, [129]);
  wsp_encode_test(MMS.SenderVisibilityValue, 130, null, "CodeError");
  wsp_encode_test(MMS.SenderVisibilityValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: StatusValue
//

//// StatusValue.decode ////

add_test(function test_StatusValue_decode() {
  // Valid codes are 128 - 135. Check boundary conditions 0, 1, 127, 136 and
  // 255 as well.
  wsp_decode_test(MMS.StatusValue, [0], null, "CodeError");
  wsp_decode_test(MMS.StatusValue, [1], null, "CodeError");
  wsp_decode_test(MMS.StatusValue, [127], null, "CodeError");
  for (let i = 128; i <= 135; i++) {
    wsp_decode_test(MMS.StatusValue, [i], i);
  }
  wsp_decode_test(MMS.StatusValue, [136], null, "CodeError");
  wsp_decode_test(MMS.StatusValue, [255], null, "CodeError");

  run_next_test();
});

//// StatusValue.encode ////

add_test(function test_StatusValue_encode() {
  // Valid codes are 128 - 135. Check boundary conditions 0, 1, 127, 136 and
  // 255 as well.
  wsp_encode_test(MMS.StatusValue, 0, null, "CodeError");
  wsp_encode_test(MMS.StatusValue, 1, null, "CodeError");
  wsp_encode_test(MMS.StatusValue, 127, null, "CodeError");
  for (let i = 128; i <= 135; i++) {
    wsp_encode_test(MMS.StatusValue, i, [i]);
  }
  wsp_encode_test(MMS.StatusValue, 136, null, "CodeError");
  wsp_encode_test(MMS.StatusValue, 255, null, "CodeError");

  run_next_test();
});

//
// Test target: PduHelper
//

//// PduHelper.parseHeaders ////

add_test(function test_PduHelper_parseHeaders() {
  function parse(input, expect, exception) {
    let data = {array: input, offset: 0};
    do_check_throws(wsp_test_func.bind(null, MMS.PduHelper.parseHeaders, data, expect),
                    exception);
  }

  // Parse ends with Content-Type
  let expect = {};
  expect["x-mms-mms-version"] = MMS_VERSION_1_3;
  expect["content-type"] = {
    media: "application/vnd.wap.multipart.related",
    params: null,
  };
  parse([0x80 | 0x0D, 0x80 | MMS_VERSION_1_3,   // X-Mms-Mms-Version: 1.3
         0x80 | 0x04, 0x80 | 0x33,          // Content-Type: application/vnd.wap.multipart.related
         0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ // X-Mms-Message-Type: M-Send.req
        ], expect);

  // Parse header fields with multiple entries
  expect = {
    to: [
      { address: "+123", type: "PLMN" },
      { address: "+456", type: "num" },
    ],
  };
  expect["content-type"] = {
    media: "application/vnd.wap.multipart.related",
    params: null,
  };
  parse(Array.concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
             .concat([0x80 | 0x17]).concat(strToCharCodeArray("+456"))
             .concat([0x80 | 0x04, 0x80 | 0x33]),
        expect);

  run_next_test();
});

//// PduHelper.encodeHeader ////

add_test(function test_PduHelper_encodeHeader() {
  function func(name, data, headers) {
    MMS.PduHelper.encodeHeader(data, headers, name);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  // Encode header fields with multiple entries
  let headers = {
    to: [
      { address: "+123", type: "PLMN" },
      { address: "+456", type: "num" },
    ],
  };
  wsp_encode_test_ex(func.bind(null, "to"), headers,
                     Array.concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
                          .concat([0x80 | 0x17]).concat(strToCharCodeArray("+456")));

  run_next_test();
});

//// PduHelper.encodeHeaderIfExists ////

add_test(function test_PduHelper_encodeHeaderIfExists() {
  function func(name, data, headers) {
    MMS.PduHelper.encodeHeaderIfExists(data, headers, name);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  wsp_encode_test_ex(func.bind(null, "to"), {}, []);

  run_next_test();
});

//// PduHelper.encodeHeaders ////

add_test(function test_PduHelper_encodeHeaders() {
  function func(data, headers) {
    MMS.PduHelper.encodeHeaders(data, headers);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  let headers = {};
  headers["x-mms-message-type"] = MMS_PDU_TYPE_SEND_REQ;
  headers["x-mms-mms-version"] = MMS_VERSION_1_3;
  headers["x-mms-transaction-id"] = "asdf";
  headers["to"] = { address: "+123", type: "PLMN" };
  headers["content-type"] = {
    media: "application/vnd.wap.multipart.related",
  };
  wsp_encode_test_ex(func, headers,
                     Array.concat([0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ])
                          .concat([0x80 | 0x18]).concat(strToCharCodeArray(headers["x-mms-transaction-id"]))
                          .concat([0x80 | 0x0D, 0x80 | MMS_VERSION_1_3])
                          .concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
                          .concat([0x80 | 0x04, 0x80 | 0x33]));

  run_next_test();
});
