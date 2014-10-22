"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function run_test() {
  run_next_test();
}

add_test(function test_valid_network_id() {
  var networkId = "networkId";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.networkId = networkId;

  equal(data.networkId, networkId);

  run_next_test();
});

add_test(function test_empty_network_id() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.networkId = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_transport_stream_id() {
  var transportStreamId = "transportStreamId";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.transportStreamId = transportStreamId;

  equal(data.transportStreamId, transportStreamId);

  run_next_test();
});

add_test(function test_empty_transport_stream_id() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.transportStreamId = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_service_id() {
  var serviceId = "serviceId";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.serviceId = serviceId;

  equal(data.serviceId, serviceId);

  run_next_test();
});

add_test(function test_empty_service_id() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.serviceId = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_type() {
  var type = "tv";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.type = type;

  equal(data.type, type);

  run_next_test();
});

add_test(function test_empty_type() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.type = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_invalid_type() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.type = "invalid";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_number() {
  var number = "number";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.number = number;

  equal(data.number, number);

  run_next_test();
});

add_test(function test_empty_number() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.number = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_name() {
  var name = "name";

  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.name = name;

  equal(data.name, name);

  run_next_test();
});

add_test(function test_empty_name() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  Assert.throws(function() {
    data.name = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_is_emergency() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.isEmergency = true;

  ok(data.isEmergency);

  run_next_test();
});

add_test(function test_is_free() {
  var data = Cc["@mozilla.org/tv/tvchanneldata;1"].
             createInstance(Ci.nsITVChannelData);
  data.isFree = true;

  ok(data.isFree);

  run_next_test();
});
