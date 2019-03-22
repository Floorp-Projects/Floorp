/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await BrowserTestUtils.withNewTab(
    {gBrowser, url: "about:blank"}, async function(browser) {
      const BASE1 = getRootDirectory(gTestPath)
            .replace("chrome://mochitests/content", "http://example.com");
      const BASE2 = getRootDirectory(gTestPath)
            .replace("chrome://mochitests/content", "http://test1.example.com");
      const URL = BASE1 + "onload_message.html";
      let sixth = BrowserTestUtils.waitForNewTab(gBrowser, URL + "#sixth", true, true);
      await ContentTask.spawn(browser, {base1: BASE1, base2: BASE2},
                              async function({base1, base2}) {
        let top = content.window;
        top.name = "top";
        top.location.href += "#top";

        let contexts = {
          top: top.location.href,
          first: base1 + "dummy_page.html#first",
          third: base2 + "dummy_page.html#third",
          second: base1 + "dummy_page.html#second",
          fourth: base2 + "dummy_page.html#fourth",
          fifth: base1 + "dummy_page.html#fifth",
          sixth: base1 + "onload_message.html#sixth",
        };

        function addFrame(target, name) {
          let doc = (target.contentWindow || target).document;
          let frame = doc.createElement("iframe");
          let p = new Promise(resolve => (frame.onload = () => resolve(frame)));
          doc.body.appendChild(frame);
          frame.name = name;
          frame.src = contexts[name];
          return p;
        }

        function addWindow(target, name) {
          var win = target.contentWindow.open(contexts[name], name);

          return new Promise(
              resolve => target.contentWindow.addEventListener(
                  "message", () => resolve(win)));
        }

        // Generate all lists of length length with every combination of
        // values in input
        function* generate(input, length) {
          let list = new Array(length);

          function* values(pos) {
            if (pos >= list.length) {
              yield list;
            } else {
              for (let v of input) {
                list[pos] = v;
                yield* values(pos + 1);
              }
            }
          }
          yield* values(0);
        }

        // We're going to create a tree that looks like the
        // follwing.
        //
        //           top          sixth
        //          /   \
        //         /     \        /
        //      first  second
        //      /   \           /
        //     /     \
        //  third  fourth - - -
        //          /
        //         /
        //      fifth
        //
        // The idea is to have one top level non-auxiliary browsing
        // context, five nested, one top level auxiliary with an
        // opener. Given that set of related browsing contexts we
        // wish to confirm that targeting is semantically equivalent
        // with how nsIDocShellTreeItem.findItemWithName works. The
        // trick to ensure that is to give all frames the same name!
        // and ensure that the find algorithms return the same nodes
        // in the same order.

        function bc(frame) {
          return (frame.contentWindow || frame).docShell.browsingContext;
        }

        let first = await addFrame(top, "first");
        let second = await addFrame(top, "second");
        let third = await addFrame(first, "third");
        let fourth = await addFrame(first, "fourth");
        let fifth = await addFrame(fourth, "fifth");
        let sixth = await addWindow(fourth, "sixth");

        let frames = [top, first, second, third, fourth, fifth, sixth];
        let browsingContexts = frames.map(bc);
        let docShells = browsingContexts.map(context => context.docShell);

        ok(top.docShell instanceof Ci.nsIDocShellTreeItem,
           "When we remove nsIDocShellTreeItem this test should be removed");

        // For every browsing context we generate all possible
        // combinations of names for these browsing contexts using
        // "dummy" and "target" as possible name.
        for (let names of generate(["dummy", "target"], docShells.length)) {
          for (let i = names.length - 1; i >= 0; --i) {
            docShells[i].name = names[i];
          }

          for (let i = 0; i < docShells.length; ++i) {
            let docShell = docShells[i].findItemWithName("target", null, null, false);
            let browsingContext = browsingContexts[i].findWithName("target");
            is(docShell ? docShell.browsingContext : null, browsingContext,
                "findItemWithName should find same browsing context as findWithName");
          }
        }

        for (let target of ["_self", "_top", "_parent", "_blank"]) {
          for (let i = 0; i < docShells.length; ++i) {
            let docShell = docShells[i].findItemWithName(target, null, null, false);
            let browsingContext = browsingContexts[i].findWithName(target);
            is(docShell ? docShell.browsingContext : null, browsingContext,
                "findItemWithName should find same browsing context as findWithName for " + target);
          }
        }
      });

      BrowserTestUtils.removeTab(await sixth);
    });
});
