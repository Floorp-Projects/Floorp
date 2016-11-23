/**
 * Boostrap LogShake's tests that need gonk support.
 * This is creating a fake sdcard for LogShake tests and importing LogShake and
 * osfile
 */

/* jshint moz: true */
/* global Components, LogCapture, LogShake, ok, add_test, run_next_test, dump,
   do_get_profile, OS, volumeService, equal, XPCOMUtils */
/* exported setup_logshake_mocks */

/* disable use strict warning */
/* jshint -W097 */

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "volumeService",
                                   "@mozilla.org/telephony/volume-service;1",
                                   "nsIVolumeService");

var sdcard;

function setup_logshake_mocks() {
  do_get_profile();
  setup_fs();
}

function setup_fs() {
  OS.File.makeDir("/data/local/tmp/sdcard/", {from: "/data"}).then(function() {
    setup_sdcard();
  });
}

function setup_sdcard() {
  let volName = "sdcard";
  let mountPoint = "/data/local/tmp/sdcard";

  let vol = volumeService.getVolumeByName(volName);
  ok(vol, "volume shouldn't be null");
  equal(volName, vol.name, "name");
  equal(Ci.nsIVolume.STATE_MOUNTED, vol.state, "state");

  ensure_sdcard();
}

function ensure_sdcard() {
  sdcard = volumeService.getVolumeByName("sdcard").mountPoint;
  ok(sdcard, "Should have a valid sdcard mountpoint");
  run_next_test();
}
