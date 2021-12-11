/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "about:blank";

async function getBrowsingContextId(browser, id) {
  return SpecialPowers.spawn(browser, [id], async function(id) {
    let contextId = content.window.docShell.browsingContext.id;

    let frames = [content.window];
    while (frames.length) {
      let frame = frames.pop();
      let target = frame.document.getElementById(id);
      if (target) {
        contextId = target.docShell.browsingContext.id;
        break;
      }

      frames = frames.concat(Array.from(frame.frames));
    }

    return contextId;
  });
}

async function addFrame(browser, id, parentId) {
  return SpecialPowers.spawn(browser, [{ parentId, id }], async function({
    parentId,
    id,
  }) {
    let parent = null;
    if (parentId) {
      let frames = [content.window];
      while (frames.length) {
        let frame = frames.pop();
        let target = frame.document.getElementById(parentId);
        if (target) {
          parent = target.contentWindow.document.body;
          break;
        }
        frames = frames.concat(Array.from(frame.frames));
      }
    } else {
      parent = content.document.body;
    }

    let frame = await new Promise(resolve => {
      let frame = content.document.createElement("iframe");
      frame.id = id || "";
      frame.url = "about:blank";
      frame.onload = () => resolve(frame);
      parent.appendChild(frame);
    });

    return frame.contentWindow.docShell.browsingContext.id;
  });
}

async function removeFrame(browser, id) {
  return SpecialPowers.spawn(browser, [id], async function(id) {
    let frames = [content.window];
    while (frames.length) {
      let frame = frames.pop();
      let target = frame.document.getElementById(id);
      if (target) {
        target.remove();
        break;
      }

      frames = frames.concat(Array.from(frame.frames));
    }
  });
}

function getBrowsingContextById(id) {
  return BrowsingContext.get(id);
}

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(
    browser
  ) {
    let topId = await getBrowsingContextId(browser, "");
    let topContext = getBrowsingContextById(topId);
    isnot(topContext, null);
    is(topContext.parent, null);
    is(
      topId,
      browser.browsingContext.id,
      "<browser> has the correct browsingContext"
    );
    is(
      browser.browserId,
      topContext.browserId,
      "browsing context should have a correct <browser> id"
    );

    let id0 = await addFrame(browser, "frame0");
    let browsingContext0 = getBrowsingContextById(id0);
    isnot(browsingContext0, null);
    is(browsingContext0.parent, topContext);

    await removeFrame(browser, "frame0");

    is(topContext.children.indexOf(browsingContext0), -1);

    // TODO(farre): Handle browsingContext removal [see Bug 1486719].
    todo_isnot(browsingContext0.parent, topContext);
  });
});

add_task(async function() {
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({ set: [["fission.bfcacheInParent", true]] });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        getRootDirectory(gTestPath).replace(
          "chrome://mochitests/content",
          "http://example.com"
        ) + "dummy_page.html",
    },
    async function(browser) {
      let path = getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        "http://example.com"
      );
      await SpecialPowers.spawn(browser, [path], async function(path) {
        var bc = new content.BroadcastChannel("browser_browsingContext");
        function waitForMessage(command) {
          let p = new Promise(resolve => {
            bc.addEventListener("message", e => resolve(e), { once: true });
          });
          command();
          return p;
        }

        // Open a new window and wait for the message.
        let e1 = await waitForMessage(_ =>
          content.window.open(path + "onpageshow_message.html", "", "noopener")
        );

        is(e1.data, "pageshow", "Got page show");

        let e2 = await waitForMessage(_ => bc.postMessage("createiframe"));
        is(e2.data.framesLength, 1, "Here we should have an iframe");

        let e3 = await waitForMessage(_ => bc.postMessage("nextpage"));

        is(e3.data.event, "load");
        is(e3.data.framesLength, 0, "Here there shouldn't be an iframe");

        // Return to the previous document. N.B. we expect to trigger
        // BFCache here, hence we wait for pageshow.
        let e4 = await waitForMessage(_ => bc.postMessage("back"));

        is(e4.data, "pageshow");

        let e5 = await waitForMessage(_ => bc.postMessage("queryframes"));
        is(e5.data.framesLength, 1, "And again there should be an iframe");

        is(e5.outerWindowId, e2.outerWindowId, "BF cache cached outer window");
        is(e5.browsingContextId, e2.browsingContextId, "BF cache cached BC");

        let e6 = await waitForMessage(_ => bc.postMessage("close"));
        is(e6.data, "closed");

        bc.close();
      });
    }
  );
});
