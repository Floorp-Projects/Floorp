/* eslint-env mozilla/frame-script */
var MIDITestUtils = {
  permissionSetup: allow => {
    let permPromiseRes;
    let permPromise = new Promise((res, rej) => {
      permPromiseRes = res;
    });
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["dom.webmidi.enabled", true],
          ["midi.testing", true],
          ["midi.prompt.testing", true],
          ["media.navigator.permission.disabled", allow],
        ],
      },
      () => {
        permPromiseRes();
      }
    );
    return permPromise;
  },
  // This list needs to stay synced with the ports in
  // dom/midi/TestMIDIPlatformService.
  inputInfo: {
    get id() {
      return MIDITestUtils.stableId(this);
    },
    name: "Test Control MIDI Device Input Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  outputInfo: {
    get id() {
      return MIDITestUtils.stableId(this);
    },
    name: "Test Control MIDI Device Output Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  stateTestInputInfo: {
    get id() {
      return MIDITestUtils.stableId(this);
    },
    name: "Test State MIDI Device Input Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  stateTestOutputInfo: {
    get id() {
      return MIDITestUtils.stableId(this);
    },
    name: "Test State MIDI Device Output Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  alwaysClosedTestOutputInfo: {
    get id() {
      return MIDITestUtils.stableId(this);
    },
    name: "Always Closed MIDI Device Output Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  checkPacket: (expected, actual) => {
    if (expected.length != actual.length) {
      ok(false, "Packet " + expected + " length not same as packet " + actual);
    }
    for (var i = 0; i < expected.length; ++i) {
      is(expected[i], actual[i], "Packet value " + expected[i] + " matches.");
    }
  },
  stableId: info => {
    // This computes the stable ID of a MIDI port according to the logic we
    // use in the Web MIDI implementation. See MIDIPortChild::GenerateStableId()
    // and nsContentUtils::AnonymizeId().
    const Cc = SpecialPowers.Cc;
    const Ci = SpecialPowers.Ci;
    const id = info.name + info.manufacturer + info.version;
    const encoder = new TextEncoder();
    const data = encoder.encode(id);
    let key = self.origin;

    var digest;
    let keyObject = Cc["@mozilla.org/security/keyobjectfactory;1"]
      .getService(Ci.nsIKeyObjectFactory)
      .keyFromString(Ci.nsIKeyObject.HMAC, key);
    let cryptoHMAC = Cc["@mozilla.org/security/hmac;1"].createInstance(
      Ci.nsICryptoHMAC
    );

    cryptoHMAC.init(Ci.nsICryptoHMAC.SHA256, keyObject);
    cryptoHMAC.update(data, data.length);
    return cryptoHMAC.finish(true);
  },
};
