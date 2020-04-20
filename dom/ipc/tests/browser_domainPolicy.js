// This test waits for a lot of subframe loads, causing it to take a long time,
// especially with Fission enabled.
requestLongerTimeout(2);

const BASE_FILE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_domainPolicy_base.html";
const SCRIPT_PATH = "/browser/dom/ipc/tests/file_disableScript.html";

const TEST_POLICY = {
  exceptions: ["http://test1.example.com", "http://example.com"],
  superExceptions: ["http://test2.example.org", "https://test1.example.com"],
  exempt: [
    "http://test1.example.com",
    "http://example.com",
    "http://test2.example.org",
    "http://sub1.test2.example.org",
    "https://sub1.test1.example.com",
  ],
  notExempt: [
    "http://test2.example.com",
    "http://sub1.test1.example.com",
    "http://www.example.com",
    "https://test2.example.com",
    "https://example.com",
    "http://test1.example.org",
  ],
};

// To make sure we never leave up an activated domain policy after a failed
// test, let's make this global.
var policy;

function activateDomainPolicy(isBlock) {
  policy = Services.scriptSecurityManager.activateDomainPolicy();

  if (isBlock === undefined) {
    return;
  }

  let set = isBlock ? policy.blocklist : policy.allowlist;
  for (let e of TEST_POLICY.exceptions) {
    set.add(makeURI(e));
  }

  let superSet = isBlock ? policy.superBlocklist : policy.superAllowlist;
  for (let e of TEST_POLICY.superExceptions) {
    superSet.add(makeURI(e));
  }
}

function deactivateDomainPolicy() {
  if (policy) {
    policy.deactivate();
    policy = null;
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.browser_frames.oop_by_default", false],
      ["browser.pagethumbnails.capturing_disabled", false],
      ["dom.mozBrowserFramesEnabled", false],
    ],
  });

  registerCleanupFunction(() => {
    deactivateDomainPolicy();
  });
});

add_task(async function test_domainPolicy() {
  function test(testFunc, { activateFirst, isBlock }) {
    if (activateFirst) {
      activateDomainPolicy(isBlock);
    }
    return BrowserTestUtils.withNewTab(
      {
        gBrowser,
        opening: BASE_FILE,
        forceNewProcess: true,
      },
      async browser => {
        if (!activateFirst) {
          activateDomainPolicy(isBlock);
        }
        await testFunc(browser);
        deactivateDomainPolicy();
      }
    );
  }

  async function testDomain(browser, domain, expectEnabled = false) {
    function navigateFrame() {
      let url = domain + SCRIPT_PATH;
      return SpecialPowers.spawn(browser, [url], async src => {
        let iframe = content.document.getElementById("root");
        await new Promise(resolve => {
          iframe.addEventListener("load", resolve, { once: true });
          iframe.src = src;
        });
        return iframe.browsingContext;
      });
    }

    function checkScriptEnabled(bc) {
      return SpecialPowers.spawn(bc, [expectEnabled], enabled => {
        content.wrappedJSObject.gFiredOnclick = false;
        content.document.body.dispatchEvent(new content.Event("click"));
        Assert.equal(
          content.wrappedJSObject.gFiredOnclick,
          enabled,
          `Checking script-enabled for ${content.name} (${content.location})`
        );
      });
    }

    let browsingContext = await navigateFrame();
    return checkScriptEnabled(browsingContext);
  }

  async function testList(browser, list, expectEnabled) {
    // Run these sequentially to avoid navigating multiple domains at once.
    for (let domain of list) {
      await testDomain(browser, domain, expectEnabled);
    }
  }

  info("1. Testing simple blocklist policy");

  info("1A. Creating child process first, activating domainPolicy after");
  await test(
    async browser => {
      policy.blocklist.add(Services.io.newURI("http://example.com"));
      await testDomain(browser, "http://example.com");
    },
    { activateFirst: false }
  );

  info("1B. Activating domainPolicy first, creating child process after");
  await test(
    async browser => {
      policy.blocklist.add(Services.io.newURI("http://example.com"));
      await testDomain(browser, "http://example.com");
    },
    { activateFirst: true }
  );

  info("2. Testing Blocklist-style Domain Policy");

  info("2A. Activating domainPolicy first, creating child process after");
  await test(
    async browser => {
      await testList(browser, TEST_POLICY.notExempt, true);
      await testList(browser, TEST_POLICY.exempt, false);
    },
    { activateFirst: true, isBlock: true }
  );

  info("2B. Creating child process first, activating domainPolicy after");
  await test(
    async browser => {
      await testList(browser, TEST_POLICY.notExempt, true);
      await testList(browser, TEST_POLICY.exempt, false);
    },
    { activateFirst: false, isBlock: true }
  );

  info("3. Testing Allowlist-style Domain Policy");
  await SpecialPowers.pushPrefEnv({ set: [["javascript.enabled", false]] });

  info("3A. Activating domainPolicy first, creating child process after");
  await test(
    async browser => {
      await testList(browser, TEST_POLICY.notExempt, false);
      await testList(browser, TEST_POLICY.exempt, true);
    },
    { activateFirst: true, isBlock: false }
  );

  info("3B. Creating child process first, activating domainPolicy after");
  await test(
    async browser => {
      await testList(browser, TEST_POLICY.notExempt, false);
      await testList(browser, TEST_POLICY.exempt, true);
    },
    { activateFirst: false, isBlock: false }
  );

  finish();
});
