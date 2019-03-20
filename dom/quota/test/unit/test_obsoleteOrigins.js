/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is mainly to verify the 2_1To2_2 upgrade does remove obsolete
// origins and the temporary storage initialization won't be blocked after
// removing them. Also, an invalid origin shouldn't block upgrades, so this
// test verifies that as well.

async function testSteps()
{
  const obsoleteOriginPaths = [
    "storage/default/chrome+++content+browser.xul/",
    "storage/default/moz-safe-about+++home/"
  ];

  info("Verifying the obsolete origins are removed after the 2_1To2_2 upgrade");

  // This profile only contains the storage.sqlite with schema 2_1, so that we
  // can test the 2_1To2_2 upgrade.
  installPackage("version2_2upgrade_profile");

  for (let originPath of obsoleteOriginPaths) {
    let originDir = getRelativeFile(originPath);
    originDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
  }

  info("Upgrading");

  let request = init();
  await requestFinished(request);

  info("Verifying the obsolete origins are removed after the 2_1To2_2 upgrade");

  for (let originPath of obsoleteOriginPaths) {
     let folder = getRelativeFile(originPath);
     let exists = folder.exists();
     ok(!exists, "Obsolete origin was removed during origin initialization");
  }

  info("Verifying the temporary storage is able to be initialized after " +
       "removing an invalid origin");

  request = initTemporaryStorage();
  await requestFinished(request);
}
