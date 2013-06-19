/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let SL = {};
subscriptLoader.loadSubScript("resource://gre/modules/SlPduHelper.jsm", SL);
SL.debug = do_print;

function run_test() {
  run_next_test();
}

/**
 * SL in plain text
 */
add_test(function test_sl_parse_plain_text() {
  let contentType = "";
  let data = {};

  contentType = "text/vnd.wap.sl";
  data.array = new Uint8Array([
                  0x3C, 0x3F, 0x78, 0x6D, 0x6C, 0x20, 0x76, 0x65,
                  0x72, 0x73, 0x69, 0x6F, 0x6E, 0x3D, 0x27, 0x31,
                  0x2E, 0x30, 0x27, 0x3F, 0x3E, 0x0A, 0x3C, 0x73,
                  0x6C, 0x20, 0x68, 0x72, 0x65, 0x66, 0x3D, 0x27,
                  0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x77,
                  0x77, 0x77, 0x2E, 0x6F, 0x72, 0x65, 0x69, 0x6C,
                  0x6C, 0x79, 0x2E, 0x63, 0x6F, 0x6D, 0x27, 0x2F,
                  0x3E
                ]);
  data.offset = 0;
  let result = "<?xml version='1.0'?>\n<sl href='http://www.oreilly.com'/>";
  let msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML
 */
add_test(function test_sl_parse_wbxml() {
  let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
                  0x03, 0x06, 0x6A, 0x00, 0x85, 0x0A, 0x03, 0x6F,
                  0x72, 0x65, 0x69, 0x6C, 0x6C, 0x79, 0x00, 0x85,
                  0x01
                ]);
  data.offset = 0;
  let result = "<sl href=\"http://www.oreilly.com/\"/>";
  let msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML, with public ID stored in string table
 */
add_test(function test_sl_parse_wbxml_public_id_string_table() {
    let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
                  0x03, 0x00, 0x00, 0x6A, 0x1C, 0x2D, 0x2F, 0x2F,
                  0x57, 0x41, 0x50, 0x46, 0x4F, 0x52, 0x55, 0x4D,
                  0x2F, 0x2F, 0x44, 0x54, 0x44, 0x20, 0x53, 0x4C,
                  0x20, 0x31, 0x2E, 0x30, 0x2F, 0x2F, 0x45, 0x4E,
                  0x00, 0x85, 0x0A, 0x03, 0x6F, 0x72, 0x65, 0x69,
                  0x6C, 0x6C, 0x79, 0x00, 0x85, 0x01
                ]);
  data.offset = 0;
  let result = "<sl href=\"http://www.oreilly.com/\"/>";
  let msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});

/**
 * SL compressed by WBXML with string table
 */
add_test(function test_sl_parse_wbxml_with_string_table() {
  let msg = {};
  let contentType = "";
  let data = {};

  contentType = "application/vnd.wap.slc";
  data.array = new Uint8Array([
                  0x03, 0x06, 0x6A, 0x08, 0x6F, 0x72, 0x65, 0x69,
                  0x6C, 0x6C, 0x79, 0x00, 0x85, 0x0A, 0x83, 0x00,
                  0x85, 0x01
                ]);
  data.offset = 0;
  let result = "<sl href=\"http://www.oreilly.com/\"/>";
  let msg = SL.PduHelper.parse(data, contentType);
  do_check_eq(msg.content, result);

  run_next_test();
});
