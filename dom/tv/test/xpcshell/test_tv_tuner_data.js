"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function run_test() {
  run_next_test();
}

add_test(function test_valid_id() {
  var id = "id";

  var data = Cc["@mozilla.org/tv/tvtunerdata;1"].
             createInstance(Ci.nsITVTunerData);
  data.id = id;

  equal(data.id, id);

  run_next_test();
});

add_test(function test_empty_id() {
  var data = Cc["@mozilla.org/tv/tvtunerdata;1"].
             createInstance(Ci.nsITVTunerData);
  Assert.throws(function() {
    data.id = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_supported_source_types() {
  var sourceTypes = ["dvb-t", "dvb-s"];

  var data = Cc["@mozilla.org/tv/tvtunerdata;1"].
             createInstance(Ci.nsITVTunerData);
  data.setSupportedSourceTypes(sourceTypes.length, sourceTypes);

  var types = data.getSupportedSourceTypes();
  equal(types.length, sourceTypes.length);
  for (var i = 0; i < types.length; i++) {
    equal(types[i], sourceTypes[i]);
  }

  run_next_test();
});

add_test(function test_empty_supported_source_types() {
  var data = Cc["@mozilla.org/tv/tvtunerdata;1"].
             createInstance(Ci.nsITVTunerData);
  Assert.throws(function() {
    data.setSupportedSourceTypes(0, []);
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_mismatched_supported_source_types() {
  var data = Cc["@mozilla.org/tv/tvtunerdata;1"].
             createInstance(Ci.nsITVTunerData);
  Assert.throws(function() {
    data.setSupportedSourceTypes(1, []);
  }, /NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY/i);

  run_next_test();
});
