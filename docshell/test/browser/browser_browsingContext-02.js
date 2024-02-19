/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function (browser) {
      const BASE1 = getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://example.com"
      );
      const BASE2 = getRootDirectory(gTestPath).replace(
        "chrome://mochitests/content",
        // eslint-disable-next-line @microsoft/sdl/no-insecure-url
        "http://test1.example.com"
      );
      const URL = BASE1 + "onload_message.html";
      let sixth = BrowserTestUtils.waitForNewTab(
        gBrowser,
        URL + "#sixth",
        true,
        true
      );
      let seventh = BrowserTestUtils.waitForNewTab(
        gBrowser,
        URL + "#seventh",
        true,
        true
      );
      let browserIds = await SpecialPowers.spawn(
        browser,
        [{ base1: BASE1, base2: BASE2 }],
        async function ({ base1, base2 }) {
          let top = content;
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
            return content.SpecialPowers.spawn(
              target,
              [name, contexts[name]],
              async (name, context) => {
                let doc = this.content.document;

                let frame = doc.createElement("iframe");
                doc.body.appendChild(frame);
                frame.name = name;
                frame.src = context;
                await new Promise(resolve => {
                  frame.addEventListener("load", resolve, { once: true });
                });
                return frame.browsingContext;
              }
            );
          }

          function addWindow(target, name, { options, resolve }) {
            return content.SpecialPowers.spawn(
              target,
              [name, contexts[name], options, resolve],
              (name, context, options, resolve) => {
                let win = this.content.open(context, name, options);
                let bc = win && win.docShell.browsingContext;

                if (resolve) {
                  return new Promise(resolve =>
                    this.content.addEventListener("message", () => resolve(bc))
                  );
                }
                return Promise.resolve({ name });
              }
            );
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

          // WindowGlobalChild.findBrowsingContextWithName requires access
          // checks, which can only be performed in the process of the accessor
          // WindowGlobalChild.
          function findWithName(bc, name) {
            return content.SpecialPowers.spawn(bc, [name], name => {
              return content.windowGlobalChild.findBrowsingContextWithName(
                name
              );
            });
          }

          async function reachable(start, target) {
            info(start.name, target.name);
            is(
              await findWithName(start, target.name),
              target,
              [start.name, "can reach", target.name].join(" ")
            );
          }

          async function unreachable(start, target) {
            is(
              await findWithName(start, target.name),
              null,
              [start.name, "can't reach", target.name].join(" ")
            );
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
          let seventh = await addWindow(fourth, "seventh", {
            options: ["noopener"],
          });
          info("seventh");

          let origin1 = [first, second, fifth, sixth];
          let origin2 = [third, fourth];

          let topBC = BrowsingContext.getFromWindow(top);
          let frames = new Map([
            [topBC, [topBC, first, second, third, fourth, fifth, sixth]],
            [first, [topBC, ...origin1, third, fourth]],
            [second, [topBC, ...origin1, third, fourth]],
            [third, [topBC, ...origin2, fifth, sixth]],
            [fourth, [topBC, ...origin2, fifth, sixth]],
            [fifth, [topBC, ...origin1, third, fourth]],
            [sixth, [...origin1, third, fourth]],
          ]);

          for (let [start, accessible] of frames) {
            for (let frame of frames.keys()) {
              if (accessible.includes(frame)) {
                await reachable(start, frame);
              } else {
                await unreachable(start, frame);
              }
            }
            await unreachable(start, seventh);
          }

          let topBrowserId = topBC.browserId;
          Assert.greater(topBrowserId, 0, "Should have a browser ID.");
          for (let [name, bc] of Object.entries({
            first,
            second,
            third,
            fourth,
            fifth,
          })) {
            is(
              bc.browserId,
              topBrowserId,
              `${name} frame should have the same browserId as top.`
            );
          }

          Assert.greater(sixth.browserId, 0, "sixth should have a browserId.");
          isnot(
            sixth.browserId,
            topBrowserId,
            "sixth frame should have a different browserId to top."
          );

          return [topBrowserId, sixth.browserId];
        }
      );

      [sixth, seventh] = await Promise.all([sixth, seventh]);

      is(
        browser.browserId,
        browserIds[0],
        "browser should have the right browserId."
      );
      is(
        browser.browsingContext.browserId,
        browserIds[0],
        "browser's BrowsingContext should have the right browserId."
      );
      is(
        sixth.linkedBrowser.browserId,
        browserIds[1],
        "sixth should have the right browserId."
      );
      is(
        sixth.linkedBrowser.browsingContext.browserId,
        browserIds[1],
        "sixth's BrowsingContext should have the right browserId."
      );

      for (let tab of [sixth, seventh]) {
        BrowserTestUtils.removeTab(tab);
      }
    }
  );
});
