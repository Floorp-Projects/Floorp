/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  yield setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks([
    {url: "http://example.com/#7", title: ""},
    {url: "http://example.com/#8", title: "title"},
    {url: "http://example.com/#9", title: "http://example.com/#9"}
  ]);

  yield addNewTabPageTab();
  checkGrid("7p,8p,9p,0,1,2,3,4,5");

  checkTooltip(0, "http://example.com/#7", "1st tooltip is correct");
  checkTooltip(1, "title\nhttp://example.com/#8", "2nd tooltip is correct");
  checkTooltip(2, "http://example.com/#9", "3rd tooltip is correct");
}

function checkTooltip(aIndex, aExpected, aMessage) {
  let link = getCell(aIndex).node.querySelector(".newtab-link");
  is(link.getAttribute("title"), aExpected, aMessage);
}
