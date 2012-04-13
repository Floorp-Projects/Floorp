/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8");
  NewTabUtils.pinnedLinks._links = [
    {url: "about:blank#7", title: ""},
    {url: "about:blank#8", title: "title"},
    {url: "about:blank#9", title: "about:blank#9"}
  ];

  yield addNewTabPageTab();
  checkGrid("7p,8p,9p,0,1,2,3,4,5");

  checkTooltip(0, "about:blank#7", "1st tooltip is correct");
  checkTooltip(1, "title\nabout:blank#8", "2nd tooltip is correct");
  checkTooltip(2, "about:blank#9", "3rd tooltip is correct");
}

function checkTooltip(aIndex, aExpected, aMessage) {
  let link = getCell(aIndex).node.querySelector(".newtab-link");
  is(link.getAttribute("title"), aExpected, aMessage);
}
