/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function test() {
  const URL = "data:text/html,<iframe width='700' height='700'></iframe>";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      let context = await SpecialPowers.spawn(browser, [], function () {
        let iframe = content.document.querySelector("iframe");
        iframe.src = "https://expired.example.com/";
        return BrowsingContext.getFromWindow(iframe.contentWindow);
      });
      await TestUtils.waitForCondition(() => {
        let frame = context.currentWindowGlobal;
        return frame && frame.documentURI.spec.startsWith("about:certerror");
      });
      await SpecialPowers.spawn(context, [], async function () {
        await ContentTaskUtils.waitForCondition(
          () => content.document.readyState == "interactive"
        );
        let aP = content.document.getElementById("badCertAdvancedPanel");
        Assert.ok(aP, "Advanced content should exist");
        Assert.ok(
          ContentTaskUtils.isHidden(aP),
          "Advanced content should not be visible by default"
        );
      });
    }
  );
});
