"use strict";

add_task(async function invalidArgument() {
  const args = [undefined, null, 42, "foo"];

  for (const argument of args) {
    let hadException = false;
    try {
      ChromeUtils.isDOMObject(argument);
    } catch (ex) {
      hadException = true;
    }
    ok(hadException, "Should have got an exception!");
  }
});

add_task(async function NoUnwrap() {
  const args = [
    window.document,
    window.document.childNodes,
    new DocumentFragment(),
    new Response(),
    new URL("https://example.com"),
  ];

  for (const argument of args) {
    ok(
      ChromeUtils.isDOMObject(argument, false),
      `${ChromeUtils.getClassName(
        argument
      )} to be a DOM object with unwrap=false`
    );
  }

  ok(
    !ChromeUtils.isDOMObject(window, false),
    `${ChromeUtils.getClassName(
      window
    )} not to be a DOM object with unwrap=false`
  );
});

add_task(async function DOMObjects() {
  const args = [
    window,
    window.document,
    window.document.childNodes,
    new DocumentFragment(),
    new Response(),
    new URL("https://example.com"),
  ];

  for (const argument of args) {
    ok(
      ChromeUtils.isDOMObject(argument),
      `${ChromeUtils.getClassName(argument)} to be a DOM object`
    );
  }
});

add_task(async function nonDOMObjects() {
  const args = [new Object(), {}, []];

  for (const argument of args) {
    ok(
      !ChromeUtils.isDOMObject(argument),
      `${ChromeUtils.getClassName(argument)} not to be a DOM object`
    );
  }
});

add_task(async function DOMObjects_contentProcess() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,<div>`,
    },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], async () => {
        const args = [
          content,
          content.document,
          content.document.querySelector("div"),
          content.document.childNodes,
          new content.DocumentFragment(),
          new content.URL("https://example.com"),
        ];

        for (const argument of args) {
          ok(
            ChromeUtils.isDOMObject(argument),
            `${ChromeUtils.getClassName(
              argument
            )} in content to be a DOM object`
          );
        }

        ok(
          !ChromeUtils.isDOMObject({}),
          `${ChromeUtils.getClassName({})} in content not to be a DOM object`
        );

        // unwrap=false
        for (const argument of args) {
          ok(
            !ChromeUtils.isDOMObject(argument, false),
            `${ChromeUtils.getClassName(
              argument
            )} in content not to be a DOM object with unwrap=false`
          );
        }
      });
    }
  );
});
