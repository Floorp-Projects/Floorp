"use strict";

async function runPrefTest(
  aHTTPSOnlyPref,
  aHTTPSOnlyPrefPBM,
  aExecuteFromPBM,
  aDesc,
  aAssertURLStartsWith
) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", aHTTPSOnlyPref],
      ["dom.security.https_only_mode_pbm", aHTTPSOnlyPrefPBM],
    ],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    await ContentTask.spawn(
      browser,
      { aExecuteFromPBM, aDesc, aAssertURLStartsWith },
      // eslint-disable-next-line no-shadow
      async function({ aExecuteFromPBM, aDesc, aAssertURLStartsWith }) {
        const responseURL = await new Promise(resolve => {
          let xhr = new XMLHttpRequest();
          xhr.timeout = 1200;
          xhr.open("GET", "http://example.com");
          if (aExecuteFromPBM) {
            xhr.channel.loadInfo.originAttributes = {
              privateBrowsingId: 1,
            };
          }
          xhr.onreadystatechange = () => {
            // We don't care about the result and it's possible that
            // the requests might even succeed in some testing environments
            if (
              xhr.readyState !== XMLHttpRequest.OPENED ||
              xhr.readyState !== XMLHttpRequest.UNSENT
            ) {
              // Let's make sure this function does not get called anymore
              xhr.onreadystatechange = undefined;
              resolve(xhr.responseURL);
            }
          };
          xhr.send();
        });
        ok(responseURL.startsWith(aAssertURLStartsWith), aDesc);
      }
    );
  });
}

add_task(async function() {
  requestLongerTimeout(2);

  await runPrefTest(
    false,
    false,
    false,
    "Setting no prefs should not upgrade",
    "http://"
  );

  await runPrefTest(
    true,
    false,
    false,
    "Setting aHTTPSOnlyPref should upgrade",
    "https://"
  );

  await runPrefTest(
    false,
    true,
    false,
    "Setting aHTTPSOnlyPrefPBM should not upgrade",
    "http://"
  );

  await runPrefTest(
    false,
    false,
    true,
    "Setting aPBM should not upgrade",
    "http://"
  );

  await runPrefTest(
    true,
    true,
    false,
    "Setting aHTTPSOnlyPref and aHTTPSOnlyPrefPBM should should upgrade",
    "https://"
  );

  await runPrefTest(
    true,
    false,
    true,
    "Setting aHTTPSOnlyPref and aPBM should upgrade",
    "https://"
  );

  await runPrefTest(
    false,
    true,
    true,
    "Setting aHTTPSOnlyPrefPBM and aPBM should upgrade",
    "https://"
  );

  await runPrefTest(
    true,
    true,
    true,
    "Setting aHTTPSOnlyPref and aHTTPSOnlyPrefPBM and aPBM should upgrade",
    "https://"
  );
});
