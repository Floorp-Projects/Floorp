add_task(async function invalidArgument() {
  const arguments = [undefined, null, 42, "foo"];

  for (const argument of arguments) {
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
  const arguments = [
    window.document,
    window.document.childNodes,
    new DocumentFragment(),
    new Response(),
    new URL("https://example.com"),
  ];

  for (const argument of arguments) {
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
  const arguments = [
    window,
    window.document,
    window.document.childNodes,
    new DocumentFragment(),
    new Response(),
    new URL("https://example.com"),
  ];

  for (const argument of arguments) {
    ok(
      ChromeUtils.isDOMObject(argument),
      `${ChromeUtils.getClassName(argument)} to be a DOM object`
    );
  }
});

add_task(async function nonDOMObjects() {
  const arguments = [new Object(), {}, []];

  for (const argument of arguments) {
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
    async function(browser) {
      await SpecialPowers.spawn(browser, [], async () => {
        const arguments = [
          content,
          content.document,
          content.document.querySelector("div"),
          content.document.childNodes,
          new content.DocumentFragment(),
          new content.URL("https://example.com"),
        ];

        for (const argument of arguments) {
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
        for (const argument of arguments) {
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
