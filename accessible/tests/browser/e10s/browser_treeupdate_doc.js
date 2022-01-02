/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

const iframeSrc = `data:text/html,
  <html>
    <head>
      <meta charset='utf-8'/>
      <title>Inner Iframe</title>
    </head>
    <body id='inner-iframe'></body>
  </html>`;

addAccessibleTask(
  `
  <iframe id="iframe" src="${iframeSrc}"></iframe>`,
  async function(browser, accDoc) {
    // ID of the iframe that is being tested
    const id = "inner-iframe";

    let iframe = findAccessibleChildByID(accDoc, id);

    /* ================= Initial tree check =================================== */
    let tree = {
      role: ROLE_DOCUMENT,
      children: [],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Write iframe document ================================ */
    let reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      let docNode = content.document.getElementById("iframe").contentDocument;
      let newHTMLNode = docNode.createElement("html");
      let newBodyNode = docNode.createElement("body");
      let newTextNode = docNode.createTextNode("New Wave");
      newBodyNode.id = contentId;
      newBodyNode.appendChild(newTextNode);
      newHTMLNode.appendChild(newBodyNode);
      docNode.replaceChild(newHTMLNode, docNode.documentElement);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "New Wave",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Replace iframe HTML element ========================== */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      let docNode = content.document.getElementById("iframe").contentDocument;
      // We can't use open/write/close outside of iframe document because of
      // security error.
      let script = docNode.createElement("script");
      script.textContent = `
      document.open();
      document.write('<body id="${contentId}">hello</body>');
      document.close();`;
      docNode.body.appendChild(script);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "hello",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Replace iframe body ================================== */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      let docNode = content.document.getElementById("iframe").contentDocument;
      let newBodyNode = docNode.createElement("body");
      let newTextNode = docNode.createTextNode("New Hello");
      newBodyNode.id = contentId;
      newBodyNode.appendChild(newTextNode);
      newBodyNode.setAttribute("role", "application");
      docNode.documentElement.replaceChild(newBodyNode, docNode.body);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_APPLICATION,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "New Hello",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Open iframe document ================================= */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      // Open document.
      let docNode = content.document.getElementById("iframe").contentDocument;
      let script = docNode.createElement("script");
      script.textContent = `
      function closeMe() {
        document.write('Works?');
        document.close();
      }
      window.closeMe = closeMe;
      document.open();
      document.write('<body id="${contentId}"></body>');`;
      docNode.body.appendChild(script);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Close iframe document ================================ */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [], () => {
      // Write and close document.
      let docNode = content.document.getElementById("iframe").contentDocument;
      docNode.write("Works?");
      docNode.close();
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "Works?",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Remove HTML from iframe document ===================== */
    reorderEventPromise = waitForEvent(EVENT_REORDER, iframe);
    await invokeContentTask(browser, [], () => {
      // Remove HTML element.
      let docNode = content.document.getElementById("iframe").contentDocument;
      docNode.firstChild.remove();
    });
    let event = await reorderEventPromise;

    ok(
      event.accessible instanceof nsIAccessibleDocument,
      "Reorder should happen on the document"
    );
    tree = {
      role: ROLE_DOCUMENT,
      children: [],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Insert HTML to iframe document ======================= */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      // Insert HTML element.
      let docNode = content.document.getElementById("iframe").contentDocument;
      let html = docNode.createElement("html");
      let body = docNode.createElement("body");
      let text = docNode.createTextNode("Haha");
      body.appendChild(text);
      body.id = contentId;
      html.appendChild(body);
      docNode.appendChild(html);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "Haha",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Remove body from iframe document ===================== */
    reorderEventPromise = waitForEvent(EVENT_REORDER, iframe);
    await invokeContentTask(browser, [], () => {
      // Remove body element.
      let docNode = content.document.getElementById("iframe").contentDocument;
      docNode.documentElement.removeChild(docNode.body);
    });
    event = await reorderEventPromise;

    ok(
      event.accessible instanceof nsIAccessibleDocument,
      "Reorder should happen on the document"
    );
    tree = {
      role: ROLE_DOCUMENT,
      children: [],
    };
    testAccessibleTree(iframe, tree);

    /* ================ Insert element under document element while body missed */
    reorderEventPromise = waitForEvent(EVENT_REORDER, iframe);
    await invokeContentTask(browser, [], () => {
      let docNode = content.document.getElementById("iframe").contentDocument;
      let inputNode = (content.window.inputNode = docNode.createElement(
        "input"
      ));
      docNode.documentElement.appendChild(inputNode);
    });
    event = await reorderEventPromise;

    ok(
      event.accessible instanceof nsIAccessibleDocument,
      "Reorder should happen on the document"
    );
    tree = {
      DOCUMENT: [{ ENTRY: [] }],
    };
    testAccessibleTree(iframe, tree);

    reorderEventPromise = waitForEvent(EVENT_REORDER, iframe);
    await invokeContentTask(browser, [], () => {
      let docEl = content.document.getElementById("iframe").contentDocument
        .documentElement;
      // Remove aftermath of this test before next test starts.
      docEl.firstChild.remove();
    });
    // Make sure reorder event was fired and that the input was removed.
    await reorderEventPromise;
    tree = {
      role: ROLE_DOCUMENT,
      children: [],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Insert body to iframe document ======================= */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      // Write and close document.
      let docNode = content.document.getElementById("iframe").contentDocument;
      // Insert body element.
      let body = docNode.createElement("body");
      let text = docNode.createTextNode("Yo ho ho i butylka roma!");
      body.appendChild(text);
      body.id = contentId;
      docNode.documentElement.appendChild(body);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_DOCUMENT,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "Yo ho ho i butylka roma!",
        },
      ],
    };
    testAccessibleTree(iframe, tree);

    /* ================= Change source ======================================== */
    reorderEventPromise = waitForEvent(EVENT_REORDER, "iframe");
    await invokeSetAttribute(
      browser,
      "iframe",
      "src",
      `data:text/html,<html><body id="${id}"><input></body></html>`
    );
    event = await reorderEventPromise;

    tree = {
      INTERNAL_FRAME: [{ DOCUMENT: [{ ENTRY: [] }] }],
    };
    testAccessibleTree(event.accessible, tree);
    iframe = findAccessibleChildByID(event.accessible, id);

    /* ================= Replace iframe body on ARIA role body ================ */
    reorderEventPromise = waitForEvent(EVENT_REORDER, id);
    await invokeContentTask(browser, [id], contentId => {
      let docNode = content.document.getElementById("iframe").contentDocument;
      let newBodyNode = docNode.createElement("body");
      let newTextNode = docNode.createTextNode("New Hello");
      newBodyNode.appendChild(newTextNode);
      newBodyNode.setAttribute("role", "application");
      newBodyNode.id = contentId;
      docNode.documentElement.replaceChild(newBodyNode, docNode.body);
    });
    await reorderEventPromise;

    tree = {
      role: ROLE_APPLICATION,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: "New Hello",
        },
      ],
    };
    testAccessibleTree(iframe, tree);
  },
  { iframe: true, remoteIframe: true }
);
