SimpleTest.waitForExplicitFinish();

// This test loads in an iframe, to ensure that the navigator instance is
// loaded with the correct value of the preference.
SpecialPowers.pushPrefEnv({ set: [["dom.netinfo.enabled", true]] }, () => {
  let iframe = document.createElement("iframe");
  iframe.id = "f1";
  iframe.src = "test_navigator_iframe.html";
  document.body.appendChild(iframe);
});
