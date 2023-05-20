/*
 * Bug 1264571 - A test case of broadcast channels for first party isolation.
 */

const TEST_DOMAIN = "http://example.net/";
const TEST_PATH =
  TEST_DOMAIN + "browser/browser/components/originattributes/test/browser/";
const TEST_PAGE = TEST_PATH + "file_broadcastChannel.html";

// IsolationTestTools flushes all preferences
// hence we explicitly pref off https-first mode
async function prefOffHttpsFirstMode() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
}

async function doTest(aBrowser) {
  let response = await SpecialPowers.spawn(aBrowser, [], async function () {
    let displayItem = content.document.getElementById("display");

    // If there is nothing in the 'display', we will try to send a message to
    // the broadcast channel and wait until this message has been delivered.
    // The way that how we make sure the message is delivered is based on an
    // iframe which will reply everything it receives from the broadcast channel
    // to the current window through the postMessage. So, we can know that the
    // boradcast message is sent successfully when the window receives a message
    // from the iframe.
    if (displayItem.innerHTML === "") {
      let data = Math.random().toString();

      let receivedData = await new Promise(resolve => {
        let listenFunc = event => {
          content.removeEventListener("message", listenFunc);
          resolve(event.data);
        };

        let bc = new content.BroadcastChannel("testBroadcastChannel");

        content.addEventListener("message", listenFunc);
        bc.postMessage(data);
      });

      Assert.equal(receivedData, data, "The value should be the same.");

      return receivedData;
    }

    return displayItem.innerHTML;
  });

  return response;
}

IsolationTestTools.runTests(TEST_PAGE, doTest, null, prefOffHttpsFirstMode);
