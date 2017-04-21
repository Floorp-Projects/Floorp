function runVRTest(callback) {
  SpecialPowers.pushPrefEnv({"set" : [["dom.vr.puppet.enabled", true],
                                      ["dom.vr.require-gesture", false],
                                      ["dom.vr.test.enabled", true]]},
  () => {
    VRServiceTest = navigator.requestVRServiceTest();
    callback();
  });
}