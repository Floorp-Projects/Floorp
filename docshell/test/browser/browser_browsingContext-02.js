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
      let seventh = BrowserTestUtils.waitForNewTab(gBrowser, URL + "#seventh", true, true);
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
          seventh: base1 + "onload_message.html#seventh",
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

        function addWindow(target, name, {options, resolve}) {
          var win = target.contentWindow.open(contexts[name], name, options);

          if (resolve) {
            return new Promise(
                resolve => target.contentWindow.addEventListener(
                    "message", () => resolve(win)));
          }
          return Promise.resolve({name});
        }

        // We're going to create a tree that looks like the
        // following.
        //
        //           top          sixth    seventh
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
        // opener, and one top level without an opener. Given that
        // set of related and one unrelated browsing contexts we
        // wish to confirm that targeting is able to find
        // appropriate browsing contexts.


        function bc(frame) {
          return (frame.contentWindow || frame).docShell.browsingContext;
        }

        function reachable(start, targets) {
          for (let target of targets) {
            is(bc(start).findWithName(target.name), bc(target),
               [bc(start).name, "can reach", target.name].join(" "));
          }
        }

        function unreachable(start, target) {
          is(bc(start).findWithName(target.name), null,
             [bc(start).name, "can't reach", target.name].join(" "));
        }

        let first = await addFrame(top, "first");
        info("first");
        let second = await addFrame(top, "second");
        info("second");
        let third = await addFrame(first, "third");
        info("third");
        let fourth = await addFrame(first, "fourth");
        info("fourth");
        let fifth = await addFrame(fourth, "fifth");
        info("fifth");
        let sixth = await addWindow(fourth, "sixth", { resolve: true });
        info("sixth");
        let seventh = await addWindow(fourth, "seventh", { options: ["noopener"] });
        info("seventh");

        let frames = [top, first, second, third, fourth, fifth, sixth];
        for (let start of frames) {
          reachable(start, frames);
          unreachable(start, seventh);
        }
      });

      for (let tab of await Promise.all([sixth, seventh])) {
        BrowserTestUtils.removeTab(tab);
      }
    });
});
