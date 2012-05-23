/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var oldVal = false;
  
function devicestorage_setup() {
  SimpleTest.waitForExplicitFinish();
  try {
    oldVal = SpecialPowers.getBoolPref("device.storage.enabled");
  } catch(e) {}
  SpecialPowers.setBoolPref("device.storage.enabled", true);
  SpecialPowers.setBoolPref("device.storage.testing", true);
  SpecialPowers.setBoolPref("device.storage.prompt.testing", true);
}

function devicestorage_cleanup() {
  SpecialPowers.setBoolPref("device.storage.enabled", oldVal);
  SpecialPowers.setBoolPref("device.storage.testing", false);
  SpecialPowers.setBoolPref("device.storage.prompt.testing", false);
  SimpleTest.finish();
}

function getRandomBuffer() {
  var size = 1024;
  var buffer = new ArrayBuffer(size);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255);
  }
  return buffer;
}

function createRandomBlob() {
 return blob = new Blob([getRandomBuffer()], {type: 'binary/random'});
}

function randomFilename(l) {
    var set = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZ";
    var result = "";
    for (var i=0; i<l; i++) {
	var r = Math.floor(set.length * Math.random());
	result += set.substring(r, r + 1);
    }
    return result;
}

