/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the animation inspector works after switching targets.

const PAGE_ON_CONTENT = `data:text/html;charset=utf-8,
<!DOCTYPE html>
<style type="text/css">
  div {
    opacity: 0;
    transition-duration: 5000ms;
    transition-property: opacity;
  }

  div:hover {
    opacity: 1;
  }
</style>
<div class="anim">animation</div>
`;
const PAGE_ON_MAIN = "about:networking";

add_task(async function () {
  await pushPref("devtools.inspector.three-pane-enabled", false);

  info("Open a page that runs on the content process and has animations");
  const tab = await addTab(PAGE_ON_CONTENT);
  const { animationInspector, inspector } = await openAnimationInspector();

  info("Check the length of the initial animations of the content process");
  is(
    animationInspector.state.animations.length,
    0,
    "The length of the initial animation is correct"
  );

  info("Check whether the mutation on content process page is worked or not");
  await assertAnimationsMutation(tab, "div", animationInspector, 1);

  info("Load a page that runs on the main process");
  await navigateTo(
    PAGE_ON_MAIN,
    tab.linkedBrowser,
    animationInspector,
    inspector
  );
  await waitUntil(() => animationInspector.state.animations.length === 0);
  ok(true, "The animations are replaced");

  info("Check whether the mutation on main process page is worked or not");
  await assertAnimationsMutation(tab, "#category-http", animationInspector, 1);

  info("Load a content process page again");
  await navigateTo(
    PAGE_ON_CONTENT,
    tab.linkedBrowser,
    animationInspector,
    inspector
  );
  await waitUntil(() => animationInspector.state.animations.length === 0);
  ok(true, "The animations are replaced again");

  info("Check the mutation on content process again");
  await assertAnimationsMutation(tab, "div", animationInspector, 1);
});

async function assertAnimationsMutation(
  tab,
  selector,
  animationInspector,
  expectedAnimationCount
) {
  await hover(tab, selector);
  await waitUntil(
    () => animationInspector.state.animations.length === expectedAnimationCount
  );
  ok(true, "Animations mutation is worked");
}

async function navigateTo(uri, browser, animationInspector, inspector) {
  const previousAnimationsFront = animationInspector.animationsFront;
  const onReloaded = inspector.once("reloaded");
  const onUpdated = inspector.once("inspector-updated");
  BrowserTestUtils.startLoadingURIString(browser, uri);
  await waitUntil(
    () => previousAnimationsFront !== animationInspector.animationsFront
  );
  ok(true, "Target is switched correctly");
  await Promise.all([onReloaded, onUpdated]);
}

async function hover(tab, selector) {
  await SpecialPowers.spawn(tab.linkedBrowser, [selector], async s => {
    const element = content.wrappedJSObject.document.querySelector(s);
    InspectorUtils.addPseudoClassLock(element, ":hover", true);
  });
}
