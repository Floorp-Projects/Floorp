/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Basic check. There should be no SSBs in an empty profile.
add_task(async () => {
  let ssbs = await SiteSpecificBrowserService.list();
  Assert.equal(ssbs.length, 0);
});
