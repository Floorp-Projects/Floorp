function runVRTest(callback) {
  SpecialPowers.pushPrefEnv({"set" : [["dom.vr.puppet.enabled", true],
                                      ["dom.vr.require-gesture", false],
                                      ["dom.vr.test.enabled", true],
                                      ["dom.vr.display.enumerate.interval", 0],
                                      ["dom.vr.controller.enumerate.interval", 0]]},
  () => {
    VRServiceTest = navigator.requestVRServiceTest();
    callback();
  });
}