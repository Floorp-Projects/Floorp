/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let Cc = SpecialPowers.Cc;
let Ci = SpecialPowers.Ci;

let volumeService = Cc["@mozilla.org/telephony/volume-service;1"].getService(Ci.nsIVolumeService);
ok(volumeService, "Should have volume service");

let volName = "fake";
let mountPoint = "/data/fake/storage";
volumeService.createFakeVolume(volName, mountPoint);

let vol = volumeService.getVolumeByName(volName);
ok(vol, "volume shouldn't be null");

is(volName, vol.name, "name");
is(mountPoint, vol.mountPoint, "moutnPoint");
is(Ci.nsIVolume.STATE_INIT, vol.state, "state");


let oldMountGen = vol.mountGeneration;
volumeService.SetFakeVolumeState(volName, Ci.nsIVolume.STATE_MOUNTED);
is(Ci.nsIVolume.STATE_MOUNTED, vol.state, "state");
ok(vol.mountGeneration > oldMountGen, "mount generation should be incremented");

finish();
