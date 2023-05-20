/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing getNodeFrontSelectorsFromTopDocument

add_task(async () => {
  const html = `
    <html>
      <head>
        <meta charset="utf8">
        <title>Test</title>
      </head>
      <body>
        <header>
          <span>hello</span>
          <span>world</span>
        </header>
        <main>
          <iframe src="data:text/html,${encodeURIComponent(
            "<html><body><h2 class='frame-child'>foo</h2></body></html>"
          )}"></iframe>
        </main>
        <footer></footer>

        <test-component>
          <div slot="slot1" id="el1">content</div>
        </test-component>
        <script>
          'use strict';

          customElements.define('test-component', class extends HTMLElement {
            constructor() {
              super();
              const shadowRoot = this.attachShadow({mode: 'open'});
              shadowRoot.innerHTML = '<slot class="slot-class" name="slot1"></slot>';
            }
          });
        </script>
      </body>
    </html>`;

  const tab = await addTab("data:text/html," + encodeURIComponent(html));
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const walker = (
    await commands.targetCommand.targetFront.getFront("inspector")
  ).walker;

  const checkSelectors = (...args) =>
    checkSelectorsFromTopDocumentForNode(commands, ...args);

  let node = await getNodeFrontInFrames(["meta"], { walker });
  await checkSelectors(
    node,
    ["head > meta:nth-child(1)"],
    "Got expected selectors for the top-level meta node"
  );

  node = await getNodeFrontInFrames(["body"], { walker });
  await checkSelectors(
    node,
    ["body"],
    "Got expected selectors for the top-level body node"
  );

  node = await getNodeFrontInFrames(["header > span"], { walker });
  await checkSelectors(
    node,
    ["body > header:nth-child(1) > span:nth-child(1)"],
    "Got expected selectors for the top-level span node"
  );

  node = await getNodeFrontInFrames(["iframe"], { walker });
  await checkSelectors(
    node,
    ["body > main:nth-child(2) > iframe:nth-child(1)"],
    "Got expected selectors for the iframe node"
  );

  node = await getNodeFrontInFrames(["iframe", "body"], { walker });
  await checkSelectors(
    node,
    ["body > main:nth-child(2) > iframe:nth-child(1)", "body"],
    "Got expected selectors for the iframe body node"
  );

  const hostFront = await getNodeFront("test-component", { walker });
  const { nodes } = await walker.children(hostFront);
  const shadowRoot = nodes.find(hostNode => hostNode.isShadowRoot);
  node = await walker.querySelector(shadowRoot, ".slot-class");

  await checkSelectors(
    node,
    ["body > test-component:nth-child(4)", ".slot-class"],
    "Got expected selectors for the shadow dom node"
  );

  await commands.destroy();
});

async function checkSelectorsFromTopDocumentForNode(
  commands,
  nodeFront,
  expectedSelectors,
  assertionText
) {
  const selectors =
    await commands.inspectorCommand.getNodeFrontSelectorsFromTopDocument(
      nodeFront
    );
  is(
    JSON.stringify(selectors),
    JSON.stringify(expectedSelectors),
    assertionText
  );
}
