/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks([
    {url: "http://example7.com/", title: ""},
    {url: "http://example8.com/", title: "title"},
    {url: "http://example9.com/", title: "http://example9.com/"}
  ]);

  yield* addNewTabPageTab();
  yield* checkGrid("7p,8p,9p,0,1,2,3,4,5");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    function checkTooltip(aIndex, aExpected, aMessage) {
      let cell = content.gGrid.cells[aIndex];

      let link = cell.node.querySelector(".newtab-link");
      Assert.equal(link.getAttribute("title"), aExpected, aMessage);
    }

    checkTooltip(0, "http://example7.com/", "1st tooltip is correct");
    checkTooltip(1, "title\nhttp://example8.com/", "2nd tooltip is correct");
    checkTooltip(2, "http://example9.com/", "3rd tooltip is correct");
  });
});

