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
  stableId: async info => {
    // This computes the stable ID of a MIDI port according to the logic we
    // use in the Web MIDI implementation. See MIDIPortChild::GenerateStableId()
    // and nsContentUtils::AnonymizeId().
    const id = info.name + info.manufacturer + info.version;
    const encoder = new TextEncoder();
    const data = encoder.encode(id);
    const keyBytes = encoder.encode(self.origin);
    const key = await crypto.subtle.importKey(
      "raw",
      keyBytes,
      { name: "HMAC", hash: "SHA-256" },
      false,
      ["sign"]
    );
    const result = new Uint8Array(await crypto.subtle.sign("HMAC", key, data));
    let resultString = "";
    for (let i = 0; i < result.length; i++) {
      resultString += String.fromCharCode(result[i]);
    }
    return btoa(resultString);
  },
};
