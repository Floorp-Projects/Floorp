/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

var Cc = SpecialPowers.Cc;
var Ci = SpecialPowers.Ci;

var volumeService = Cc["@mozilla.org/telephony/volume-service;1"].getService(Ci.nsIVolumeService);
ok(volumeService, "Should have volume service");

var volName = "fake";
var mountPoint = "/data/fake/storage";
volumeService.createFakeVolume(volName, mountPoint);

var vol = volumeService.getVolumeByName(volName);
ok(vol, "volume shouldn't be null");

is(volName, vol.name, "name");
is(mountPoint, vol.mountPoint, "moutnPoint");
is(Ci.nsIVolume.STATE_MOUNTED, vol.state, "state");

ok(vol.mountGeneration > 0, "mount generation should not be zero");

finish();
