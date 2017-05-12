/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  await setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  await addNewTabPageTab();
  await checkGrid("0,1,2,3,4,5,6,7,8");

  await simulateExternalDrop(1);
  await checkGrid("0,99p,1,2,3,4,5,6,7");

  await blockCell(1);
  await checkGrid("0,1,2,3,4,5,6,7,8");

  await simulateExternalDrop(1);
  await checkGrid("0,99p,1,2,3,4,5,6,7");

  // Simulate a restart and force the next about:newtab
  // instance to read its data from the storage again.
  NewTabUtils.blockedLinks.resetCache();

  // Update all open pages, e.g. preloaded ones.
  NewTabUtils.allPages.update();

  await addNewTabPageTab();
  await checkGrid("0,99p,1,2,3,4,5,6,7");

  await blockCell(1);
  await checkGrid("0,1,2,3,4,5,6,7,8");
});
