/* Native regression reproducer. Use only in a fresh harness profile.
 * Does not require Floorp UI or CSP exceptions. No external network loads.
 */
add_task(async function empty_text_stylesheet_then_nonempty_rule() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async (browser) => {
      const result = await SpecialPowers.spawn(browser, [], async () => {
        const doc = content.document;
        const target = doc.createElement("div");
        target.id = "empty-style-utf8-regression";
        target.textContent = "test";
        const style = doc.createElement("style");
        style.append(doc.createTextNode(""), doc.createTextNode(""));
        doc.body.append(target);
        try {
          doc.head.insertBefore(style, doc.head.firstChild);
          const emptySheetCreated = !!style.sheet;
          style.append(
            doc.createTextNode(
              "#empty-style-utf8-regression { color: rgb(12, 34, 56); }",
            ),
          );
          const color = content.getComputedStyle(target).color;
          style.replaceChildren(doc.createTextNode(""));
          const clearedRuleCount = style.sheet.cssRules.length;
          style.append(
            doc.createTextNode(
              "#empty-style-utf8-regression { color: rgb(65, 43, 21); }",
            ),
          );
          return {
            emptySheetCreated,
            color,
            clearedRuleCount,
            restoredColor: content.getComputedStyle(target).color,
          };
        } finally {
          style.remove();
          target.remove();
        }
      });
      ok(
        result.emptySheetCreated,
        "Empty text nodes bind stylesheet without crashing",
      );
      is(result.color, "rgb(12, 34, 56)", "Nonempty rule still applies");
      is(result.clearedRuleCount, 0, "Clearing text removes rule");
      is(
        result.restoredColor,
        "rgb(65, 43, 21)",
        "Repopulating stylesheet retains behavior",
      );
    },
  );
});
