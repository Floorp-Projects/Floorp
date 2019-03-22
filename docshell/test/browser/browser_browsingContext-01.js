/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "about:blank";

async function getBrowsingContextId(browser, id) {
  return ContentTask.spawn(
    browser,
    id,
    async function(id) {
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
  return ContentTask.spawn(
    browser,
    {parentId, id},
    async function({ parentId, id }) {
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

      let frame = content.document.createElement("iframe");
      frame.id = id || "";
      frame.url = "about:blank";
      parent.appendChild(frame);

      return frame.contentWindow.docShell.browsingContext.id;
    });
}

async function removeFrame(browser, id) {
  return ContentTask.spawn(
    browser,
    id,
    async function(id) {
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
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL },
    async function(browser) {
      let topId = await getBrowsingContextId(browser, "");
      let topContext = getBrowsingContextById(topId);
      isnot(topContext, null);
      is(topContext.parent, null);
      is(topId, browser.browsingContext.id, "<browser> has the correct browsingContext");

      let id0 = await addFrame(browser, "frame0");
      let browsingContext0 = getBrowsingContextById(id0);
      isnot(browsingContext0, null);
      is(browsingContext0.parent, topContext);

      let id1 = await addFrame(browser, "frame1", "frame0");
      let browsingContext1 = getBrowsingContextById(id1);
      isnot(browsingContext1, null);
      is(browsingContext1.parent, browsingContext0);

      let id2 = await addFrame(browser, "frame2", "frame1");
      let browsingContext2 = getBrowsingContextById(id2);
      isnot(browsingContext2, null);
      is(browsingContext2.parent, browsingContext1);

      await removeFrame(browser, "frame2");

      is(browsingContext1.getChildren().indexOf(browsingContext2), -1);

      // TODO(farre): Handle browsingContext removal [see Bug 1486719].
      todo_isnot(browsingContext2.parent, browsingContext1);
  });
});

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com") + "dummy_page.html" },
    async function(browser) {
      let path = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
      await ContentTask.spawn(browser, path, async function(path) {
        function waitForMessage(command) {
          let r;
          let p = new Promise(resolve => {
            content.window.addEventListener("message", e => resolve({result: r, event: e}),
                                            {once: true});
          });
          r = command();
          return p;
        }

        // Open a new window and wait for the message.
        let { result: win, event: e1 } =
            await waitForMessage(
              _ => content.window.open(path + "onpageshow_message.html"));

        is(e1.data, "pageshow");

        {
          // Create, attach and load an iframe into the window's document.
          let frame = win.document.createElement("iframe");
          win.document.body.appendChild(frame);
          frame.src = "dummy_page.html";
          await ContentTaskUtils.waitForEvent(frame, "load");
        }

        is(win.frames.length, 1, "Here we should have an iframe");

        // The frame should have expected browsing context and docshell.
        let frameBC = win.frames[0].docShell.browsingContext;
        let winDocShell = win.frames[0].docShell;

        // Navigate the window and wait for the message.
        let { event: e2 } = await waitForMessage(
          _ => win.location = path + "onload_message.html");

        is(e2.data, "load");
        is(win.frames.length, 0, "Here there shouldn't be an iframe");

        // Return to the previous document. N.B. we expect to trigger
        // BFCache here, hence we wait for pageshow.
        let { event: e3 } = await waitForMessage(_ => win.history.back());

        is(e3.data, "pageshow");
        is(win.frames.length, 1, "And again there should be an iframe");

        is(winDocShell, win.frames[0].docShell, "BF cache cached docshell");
        is(frameBC, win.frames[0].docShell.browsingContext, "BF cache cached BC");
        is(frameBC.id, win.frames[0].docShell.browsingContext.id,
           "BF cached BC's have same id");
        is(win.docShell.browsingContext.getChildren()[0], frameBC,
           "BF cached BC's should still be a child of its parent");
        is(win.docShell.browsingContext, frameBC.parent,
           "BF cached BC's should still be a connected to its parent");

        win.close();
      });
  });
});
