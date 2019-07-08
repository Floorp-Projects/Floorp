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
    id: "b744eebe-f7d8-499b-872b-958f63c8f522",
    name: "Test Control MIDI Device Input Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  outputInfo: {
    id: "ab8e7fe8-c4de-436a-a960-30898a7c9a3d",
    name: "Test Control MIDI Device Output Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  stateTestInputInfo: {
    id: "a9329677-8588-4460-a091-9d4a7f629a48",
    name: "Test State MIDI Device Input Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  stateTestOutputInfo: {
    id: "478fa225-b5fc-4fa6-a543-d32d9cb651e7",
    name: "Test State MIDI Device Output Port",
    manufacturer: "Test Manufacturer",
    version: "1.0.0",
  },
  alwaysClosedTestOutputInfo: {
    id: "f87d0c76-3c68-49a9-a44f-700f1125c07a",
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
};
