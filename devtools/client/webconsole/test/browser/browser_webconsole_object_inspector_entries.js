/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check expanding/collapsing object with entries (Maps, Sets, URLSearchParams, …) in the console.
const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
  `<!DOCTYPE html><h1>Object Inspector on Object with entries</h1>`
)}`;

const { ELLIPSIS } = require("resource://devtools/shared/l10n.js");

add_task(async function () {
  // This will make it so we'll have stable MIDI devices reported
  await pushPref("midi.testing", true);
  await pushPref("dom.webmidi.enabled", true);
  await pushPref("midi.prompt.testing", true);
  await pushPref("media.navigator.permission.disabled", true);
  // enable custom highlight API
  await pushPref("dom.customHighlightAPI.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  logAllStoreChanges(hud);

  const taskResult = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async function () {
      const formData = new content.FormData();
      formData.append("a", 1);
      formData.append("a", 2);
      formData.append("b", 3);

      const midiAccess = Cu.waiveXrays(
        await content.wrappedJSObject.navigator.requestMIDIAccess()
      );

      content.CSS.highlights.set("search", new content.Highlight());
      content.CSS.highlights.set("glow", new content.Highlight());
      content.CSS.highlights.set("anchor", new content.Highlight());

      content.wrappedJSObject.console.log(
        "oi-entries-test",
        new Map(
          Array.from({ length: 2 }).map((el, i) => [
            { key: i },
            content.document,
          ])
        ),
        new Map(Array.from({ length: 20 }).map((el, i) => [Symbol(i), i])),
        new Map(Array.from({ length: 331 }).map((el, i) => [Symbol(i), i])),
        new Set(Array.from({ length: 2 }).map((el, i) => ({ value: i }))),
        new Set(Array.from({ length: 20 }).map((el, i) => i)),
        new Set(Array.from({ length: 222 }).map((el, i) => i)),
        new content.URLSearchParams([
          ["a", 1],
          ["a", 2],
          ["b", 3],
          ["b", 3],
          ["b", 5],
          ["c", "this is 6"],
          ["d", 7],
          ["e", 8],
          ["f", 9],
          ["g", 10],
          ["h", 11],
        ]),
        new content.Headers({ a: 1, b: 2, c: 3 }),
        formData,
        midiAccess.inputs,
        midiAccess.outputs,
        content.CSS.highlights
      );

      return {
        midi: {
          inputs: [...midiAccess.inputs.values()].map(input => ({
            id: input.id,
            name: input.name,
            type: input.type,
            manufacturer: input.manufacturer,
          })),
          outputs: [...midiAccess.outputs.values()].map(output => ({
            id: output.id,
            name: output.name,
            type: output.type,
            manufacturer: output.manufacturer,
          })),
        },
      };
    }
  );

  const node = await waitFor(() =>
    findConsoleAPIMessage(hud, "oi-entries-test")
  );
  const objectInspectors = [...node.querySelectorAll(".tree")];
  is(
    objectInspectors.length,
    12,
    "There is the expected number of object inspectors"
  );

  const [
    smallMapOi,
    mapOi,
    largeMapOi,
    smallSetOi,
    setOi,
    largeSetOi,
    urlSearchParamsOi,
    headersOi,
    formDataOi,
    midiInputsOi,
    midiOutputsOi,
    highlightsRegistryOi,
  ] = objectInspectors;

  await testSmallMap(smallMapOi);
  await testMap(mapOi);
  await testLargeMap(largeMapOi);
  await testSmallSet(smallSetOi);
  await testSet(setOi);
  await testLargeSet(largeSetOi);
  await testUrlSearchParams(urlSearchParamsOi);
  await testHeaders(headersOi);
  await testFormData(formDataOi);
  await testMidiInputs(midiInputsOi, taskResult.midi.inputs);
  await testMidiOutputs(midiOutputsOi, taskResult.midi.outputs);
  await testHighlightsRegistry(highlightsRegistryOi);
});

async function testSmallMap(oi) {
  info("Expanding the Map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 4 original ones, and the 2 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  info("Expand first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[3].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Map (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…} -> HTMLDocument
   * | | | ▶︎ <key>: Object {…}
   * | | | ▶︎ <value>: HTMLDocument
   * | | ▶︎ 1: {…} -> HTMLDocument
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");

  info("Expand <key> for first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[4].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Map (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…} -> HTMLDocument
   * | | | ▼ <key>: Object {…}
   * | | | |   key: 0
   * | | | | ▶︎ <prototype>
   * | | | ▶︎ <value>: HTMLDocument
   * | | ▶︎ 1: {…} -> HTMLDocument
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 10, "There is the expected number of nodes in the tree");

  info("Expand <value> for first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[7].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  ok(oiNodes.length > 10, "The document node was expanded");
}

async function testMap(oi) {
  info("Expanding the Map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 24 nodes, the 4 original ones, and the 20 entries.
  is(oiNodes.length, 24, "There is the expected number of nodes in the tree");
}

async function testLargeMap(oi) {
  info("Expanding the large map");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 8 nodes, the 4 original ones, and the 4 buckets.
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");
  is(oiNodes[3].textContent, `[0${ELLIPSIS}99]`);
  is(oiNodes[4].textContent, `[100${ELLIPSIS}199]`);
  is(oiNodes[5].textContent, `[200${ELLIPSIS}299]`);
  is(oiNodes[6].textContent, `[300${ELLIPSIS}330]`);
}

async function testSmallSet(oi) {
  info("Expanding the Set");
  let onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onMapOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the map");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 4 original ones, and the 2 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  info("Expand first entry");
  onMapOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[3].querySelector(".arrow").click();
  await onMapOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  /*
   * ▼ Set (2)
   * |   size: 2
   * | ▼ <entries>
   * | | ▼ 0: {…}
   * | | | | value: 0
   * | | | ▶︎ <prototype>
   * | | ▶︎ 1: {…}
   * | ▶︎ <prototype>
   */
  is(oiNodes.length, 8, "There is the expected number of nodes in the tree");
}

async function testSet(oi) {
  info("Expanding the Set");
  let onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onSetOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the Set");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onSetOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 24 nodes, the 4 original ones, and the 20 entries.
  is(oiNodes.length, 24, "There is the expected number of nodes in the tree");
}

async function testLargeSet(oi) {
  info("Expanding the large Set");
  let onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onSetOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  info("Expanding the <entries> leaf of the Set");
  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );
  onSetOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onSetOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 7 nodes, the 4 original ones, and the 3 buckets.
  is(oiNodes.length, 7, "There is the expected number of nodes in the tree");
  is(oiNodes[3].textContent, `[0${ELLIPSIS}99]`);
  is(oiNodes[4].textContent, `[100${ELLIPSIS}199]`);
  is(oiNodes[5].textContent, `[200${ELLIPSIS}221]`);
}

async function testUrlSearchParams(oi) {
  is(
    oi.textContent,
    `URLSearchParams(11) { a → "1", a → "2", b → "3", b → "3", b → "5", c → "this is 6", d → "7", e → "8", f → "9", g → "10", ${ELLIPSIS} }`,
    "URLSearchParams has expected content"
  );

  info("Expanding the URLSearchParams");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the URLSearchParams");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 14 nodes, the 4 original ones, and the 11 entries.
  is(oiNodes.length, 15, "There is the expected number of nodes in the tree");

  is(
    oiNodes[3].textContent,
    `0: a → "1"`,
    "First entry is displayed as expected"
  );
  is(
    oiNodes[4].textContent,
    `1: a → "2"`,
    `Second "a" entry is also display although it has the same name as the first entry`
  );
  is(
    oiNodes[5].textContent,
    `2: b → "3"`,
    `Third entry is the expected one...`
  );
  is(
    oiNodes[6].textContent,
    `3: b → "3"`,
    `As well as fourth, even though both name and value are similar`
  );
  is(
    oiNodes[7].textContent,
    `4: b → "5"`,
    `Fifth entry is displayed as expected`
  );
  is(
    oiNodes[8].textContent,
    `5: c → "this is 6"`,
    `Sixth entry is displayed as expected`
  );
}

async function testHeaders(oi) {
  is(
    oi.textContent,
    `Headers(3) { a → "1", b → "2", c → "3" }`,
    "Headers has expected content"
  );

  info("Expanding the Headers");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 3 nodes: the root, entries and the proto.
  is(oiNodes.length, 3, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[1];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the Headers");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 3 original ones, and the 3 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  is(oiNodes[2].textContent, `a: "1"`, "First entry is displayed as expected");
  is(
    oiNodes[3].textContent,
    `b: "2"`,
    `Second "a" entry is also display although it has the same name as the first entry`
  );
  is(oiNodes[4].textContent, `c: "3"`, `Third entry is the expected one...`);
}

async function testFormData(oi) {
  is(
    oi.textContent,
    `FormData(3) { a → "1", a → "2", b → "3" }`,
    "FormData has expected content"
  );

  info("Expanding the FormData");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 3 nodes: the root, entries and the proto.
  is(oiNodes.length, 3, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[1];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the FormData");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 6 nodes, the 3 original ones, and the 3 entries.
  is(oiNodes.length, 6, "There is the expected number of nodes in the tree");

  is(
    oiNodes[2].textContent,
    `0: a → "1"`,
    "First entry is displayed as expected"
  );
  is(
    oiNodes[3].textContent,
    `1: a → "2"`,
    `Second "a" entry is also display although it has the same name as the first entry`
  );
  is(
    oiNodes[4].textContent,
    `2: b → "3"`,
    `Third entry entry is displayed as expected`
  );
}

async function testMidiInputs(oi, midiInputs) {
  const [input] = midiInputs;
  is(
    oi.textContent,
    `MIDIInputMap { "${input.id}" → MIDIInput }`,
    "MIDIInputMap has expected content"
  );

  info("Expanding the MIDIInputMap");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the MIDIInputMap");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 5 nodes, the 4 original ones, and the entry.
  is(oiNodes.length, 5, "There is the expected number of nodes in the tree");

  is(
    oiNodes[3].textContent,
    `"${input.id}": MIDIInput { id: "${input.id}", manufacturer: "${input.manufacturer}", name: "${input.name}", ${ELLIPSIS} }`,
    "First entry is displayed as expected"
  );
}

async function testMidiOutputs(oi, midiOutputs) {
  is(
    oi.textContent,
    `MIDIOutputMap(3) { "${midiOutputs[0].id}" → MIDIOutput, "${midiOutputs[1].id}" → MIDIOutput, "${midiOutputs[2].id}" → MIDIOutput }`,
    "MIDIOutputMap has expected content"
  );

  info("Expanding the MIDIOutputMap");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the MIDIOutputMap");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 7 nodes, the 4 original ones, and the 3 entries.
  is(oiNodes.length, 7, "There is the expected number of nodes in the tree");

  is(
    oiNodes[3].textContent,
    `"${midiOutputs[0].id}": MIDIOutput { id: "${midiOutputs[0].id}", manufacturer: "${midiOutputs[0].manufacturer}", name: "${midiOutputs[0].name}", ${ELLIPSIS} }`,
    "First entry is displayed as expected"
  );
  is(
    oiNodes[4].textContent,
    `"${midiOutputs[1].id}": MIDIOutput { id: "${midiOutputs[1].id}", manufacturer: "${midiOutputs[1].manufacturer}", name: "${midiOutputs[1].name}", ${ELLIPSIS} }`,
    "Second entry is displayed as expected"
  );
  is(
    oiNodes[5].textContent,
    `"${midiOutputs[2].id}": MIDIOutput { id: "${midiOutputs[2].id}", manufacturer: "${midiOutputs[2].manufacturer}", name: "${midiOutputs[2].name}", ${ELLIPSIS} }`,
    "Third entry is displayed as expected"
  );
}

async function testHighlightsRegistry(oi) {
  is(
    oi.textContent,
    `HighlightRegistry(3) { search → Highlight, glow → Highlight, anchor → Highlight }`,
    "HighlightRegistry has expected content"
  );

  info("Expanding the HighlightRegistry");
  let onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  oi.querySelector(".arrow").click();
  await onOiMutation;

  ok(
    oi.querySelector(".arrow").classList.contains("expanded"),
    "The arrow of the node has the expected class after clicking on it"
  );

  let oiNodes = oi.querySelectorAll(".node");
  // There are 4 nodes: the root, size, entries and the proto.
  is(oiNodes.length, 4, "There is the expected number of nodes in the tree");

  const entriesNode = oiNodes[2];
  is(
    entriesNode.textContent,
    "<entries>",
    "There is the expected <entries> node"
  );

  info("Expanding the <entries> leaf of the HighlightRegistry");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });

  entriesNode.querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 7 nodes, the 4 original ones, and the 3 entries.
  is(oiNodes.length, 7, "There is the expected number of nodes in the tree");

  is(
    oiNodes[3].textContent,
    `0: search → Highlight { priority: 0, type: "highlight", size: 0 }`,
    "First entry is displayed as expected"
  );
  is(
    oiNodes[4].textContent,
    `1: glow → Highlight { priority: 0, type: "highlight", size: 0 }`,
    `Second entry is displayed as expected`
  );
  is(
    oiNodes[5].textContent,
    `2: anchor → Highlight { priority: 0, type: "highlight", size: 0 }`,
    `Third entry entry is displayed as expected`
  );

  info("Expand last entry");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[5].querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 9 nodes, the 7 original ones, <key> and <value>
  is(oiNodes.length, 9, "There is the expected number of nodes in the tree");
  is(oiNodes[6].textContent, `<key>: "anchor"`, `Got expected key node`);
  is(
    oiNodes[7].textContent,
    `<value>: Highlight { priority: 0, type: "highlight", size: 0 }`,
    `Got expected value node`
  );

  info("Expand Highlight object");
  onOiMutation = waitForNodeMutation(oi, {
    childList: true,
  });
  oiNodes[7].querySelector(".arrow").click();
  await onOiMutation;

  oiNodes = oi.querySelectorAll(".node");
  // There are now 13 nodes, the 9 previous ones, and all the properties of the Highlight object
  is(oiNodes.length, 13, "There is the expected number of nodes in the tree");
  is(oiNodes[8].textContent, `priority: 0`, `Got expected priority property`);
  is(oiNodes[9].textContent, `size: 0`, `Got expected size property`);
  is(
    oiNodes[10].textContent,
    `type: "highlight"`,
    `Got expected type property`
  );
  is(
    oiNodes[11].textContent,
    `<prototype>: HighlightPrototype { add: add(), clear: clear(), delete: delete(), … }`,
    `Got expected prototype property`
  );
}
