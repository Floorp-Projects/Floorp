// Bug 1662359 - Don't upgrade subresources whose triggering principal is exempt from HTTPS-Only mode.
// https://bugzilla.mozilla.org/bug/1662359
"use strict";

const TRIGGERING_PAGE = "http://example.org";
const LOADED_RESOURCE = "http://example.com";

add_task(async function() {
  // Enable HTTPS-Only Mode
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await runTest(
    "Request with not exempt triggering principal should get upgraded.",
    "https://"
  );

  // Now exempt the triggering page
  await SpecialPowers.pushPermissions([
    {
      type: "https-only-load-insecure",
      allow: true,
      context: TRIGGERING_PAGE,
    },
  ]);

  await runTest(
    "Request with exempt triggering principal should not get upgraded.",
    "http://"
  );

  await SpecialPowers.popPermissions();
});

async function runTest(desc, startsWith) {
  const responseURL = await new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", LOADED_RESOURCE);

    // Replace loadinfo with one whose triggeringPrincipal is a content
    // principal for TRIGGERING_PAGE.
    const triggeringPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      TRIGGERING_PAGE
    );
    let dummyURI = Services.io.newURI(LOADED_RESOURCE);
    let dummyChannel = NetUtil.newChannel({
      uri: dummyURI,
      triggeringPrincipal,
      loadingPrincipal: xhr.channel.loadInfo.loadingPrincipal,
      securityFlags: xhr.channel.loadInfo.securityFlags,
      contentPolicyType: xhr.channel.loadInfo.externalContentPolicyType,
    });
    xhr.channel.loadInfo = dummyChannel.loadInfo;

    xhr.onreadystatechange = () => {
      // We don't care about the result, just if Firefox upgraded the URL
      // internally.
      if (
        xhr.readyState !== XMLHttpRequest.OPENED ||
        xhr.readyState !== XMLHttpRequest.UNSENT
      ) {
        // Let's make sure this function doesn't get called anymore
        xhr.onreadystatechange = undefined;
        resolve(xhr.responseURL);
      }
    };
    xhr.send();
  });
  ok(responseURL.startsWith(startsWith), desc);
}
