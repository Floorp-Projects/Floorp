"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function run_test() {
  run_next_test();
}

add_test(function test_valid_inputport_id() {
  var inputportId = "inputportId";

  var data = Cc["@mozilla.org/inputport/inputportdata;1"].
             createInstance(Ci.nsIInputPortData);
  data.id = inputportId;

  equal(data.id, inputportId);

  run_next_test();
});

add_test(function test_empty_inputport_id() {
  var data = Cc["@mozilla.org/inputport/inputportdata;1"].
             createInstance(Ci.nsIInputPortData);
  Assert.throws(function() {
    data.id = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_type() {
  var  inputportType = "hdmi";

  var data = Cc["@mozilla.org/inputport/inputportdata;1"].
             createInstance(Ci.nsIInputPortData);
  data.type = inputportType;

  equal(data.type, inputportType);

  run_next_test();
});

add_test(function test_empty_type() {
  var data = Cc["@mozilla.org/inputport/inputportdata;1"].
             createInstance(Ci.nsIInputPortData);
  Assert.throws(function() {
    data.type = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_is_connected() {
  var data = Cc["@mozilla.org/inputport/inputportdata;1"].
             createInstance(Ci.nsIInputPortData);
  data.connected = true;

  ok(data.connected);

  run_next_test();
});
