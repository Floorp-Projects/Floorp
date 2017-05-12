/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

gDirectorySource = "data:application/json," + JSON.stringify({
  "directory": [{
    url: "http://example1.com/",
    enhancedImageURI: "data:image/png;base64,helloWORLD2",
    title: "title1",
    type: "affiliate",
    titleBgColor: "green"
  }]
});

add_task(async function() {
  await pushPrefs(["browser.newtab.preload", false]);

  // Make the page have a directory link
  await setLinks([]);
  await addNewTabPageTab();

  let color = await performOnCell(0, cell => {
    return cell.node.querySelector(".newtab-title").style.backgroundColor;
  });

  is(color, "green", "title bg color is green");
});
