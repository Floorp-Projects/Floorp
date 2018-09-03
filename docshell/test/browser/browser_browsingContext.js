/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "about:blank";

async function getBrowsingContextId(browser, id) {
  return await ContentTask.spawn(
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
  return await ContentTask.spawn(
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
  return await ContentTask.spawn(
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
  let contexts = ChromeUtils.getRootBrowsingContexts();

  while (contexts.length) {
    let context = contexts.pop();
    if (context.id === id) {
      return context;
    }
    contexts = contexts.concat(context.getChildren());
  }

  return null;
}

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL },
    async function(browser) {
      let topId = await getBrowsingContextId(browser, "");
      let topContext = getBrowsingContextById(topId);
      isnot(topContext, null);
      is(topContext.parent, null);

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
