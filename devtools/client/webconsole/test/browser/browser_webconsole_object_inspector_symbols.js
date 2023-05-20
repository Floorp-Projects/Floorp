/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object with symbol properties in the console.
const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log("oi-symbols-test", {
      [Symbol()]: "first symbol",
      [Symbol()]: "second symbol",
      [Symbol()]: 0,
      [Symbol()]: null,
      [Symbol()]: false,
      [Symbol()]: undefined,
      [Symbol("named")]: "named symbol",
      [Symbol("array")]: [1, 2, 3],
    });
  });

  const node = await waitFor(() =>
    findConsoleAPIMessage(hud, "oi-symbols-test")
  );
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    1,
    "There is the expected number of object inspectors"
  );

  const [oi] = objectInspectors;

  info("Expanding the Object");
  const onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  const oiNodes = oi.querySelectorAll(".node");
  // The object inspector should look like this:
  /*
   * ▼ { … }
   * |   Symbol(): "first symbol",
   * |   Symbol(): "second symbol",
   * |   Symbol(): 0,
   * |   Symbol(): null,
   * |   Symbol(): false,
   * |   Symbol(): undefined,
   * |   Symbol(named): "named symbol",
   * |   Symbol(array): Array(3) [ 1, 2, 3 ],
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 10, "There is the expected number of nodes in the tree");

  is(oiNodes[1].textContent.trim(), `Symbol(): "first symbol"`);
  is(oiNodes[2].textContent.trim(), `Symbol(): "second symbol"`);
  is(oiNodes[3].textContent.trim(), `Symbol(): 0`);
  is(oiNodes[4].textContent.trim(), `Symbol(): null`);
  is(oiNodes[5].textContent.trim(), `Symbol(): false`);
  is(oiNodes[6].textContent.trim(), `Symbol(): undefined`);
  is(oiNodes[7].textContent.trim(), `Symbol(named): "named symbol"`);
  is(oiNodes[8].textContent.trim(), `Symbol(array): Array(3) [ 1, 2, 3 ]`);
});
