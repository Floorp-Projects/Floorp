/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

add_task(async function test_permissions_sent_over_ipc_on_bloburl() {
  const ssm = Services.scriptSecurityManager;
  const pm = Services.perms;

  // setup a profile.
  do_get_profile();

  async function assertExpectedContentPage(contentPage) {
    const [processType, remoteType, principalSpec] = await page.spawn(
      [],
      async () => {
        return [
          Services.appinfo.processType,
          Services.appinfo.remoteType,
          this.content.document.nodePrincipal.spec,
        ];
      }
    );

    equal(
      processType,
      Services.appinfo.PROCESS_TYPE_CONTENT,
      "Got a content process"
    );
    equal(remoteType, "file", "Got a file child process");
    equal(principalSpec, principal.spec, "Got the expected document principal");
  }

  function getChildProcessID(contentPage) {
    return contentPage.spawn([], () => Services.appinfo.processID);
  }

  async function assertHasAllowedPermission(contentPage, perm) {
    const isPermissionAllowed = await contentPage.spawn(
      [perm],
      permName =>
        Services.perms.getPermissionObject(
          this.content.document.nodePrincipal,
          permName,
          true
        )?.capability === Services.perms.ALLOW_ACTION
    );
    ok(isPermissionAllowed, `Permission "${perm}" allowed as expected`);
  }

  let file = do_get_file(".", true);
  let fileURI = Services.io.newFileURI(file);
  const principal = ssm.createContentPrincipal(fileURI, {});
  info(`Add a test permission to the document principal: ${principal.spec}`);
  pm.addFromPrincipal(principal, "test/perm", pm.ALLOW_ACTION);

  info("Test expected permission is propagated into the child process");
  let page = await ExtensionTestUtils.loadContentPage(fileURI.spec);
  const childID1 = await getChildProcessID(page);
  await assertExpectedContentPage(page);
  await assertHasAllowedPermission(page, "test/perm");
  await page.close();

  // Ensure this blob url does not prevent permissions to be propagated
  // to a new child process.
  info("Create a blob url for a non http/https principal");
  const blob = new Blob();
  const blobURL = URL.createObjectURL(blob);
  ok(blobURL, "Got a blob URL");

  info("Test expected permission is still propagated");
  page = await ExtensionTestUtils.loadContentPage(fileURI.spec);
  const childID2 = await getChildProcessID(page);
  await assertExpectedContentPage(page);
  Assert.notEqual(childID1, childID2, "Got a new child process as expected");
  await assertHasAllowedPermission(page, "test/perm");
  await page.close();

  URL.revokeObjectURL(blobURL);

  page = await ExtensionTestUtils.loadContentPage(fileURI.spec);
  const childID3 = await getChildProcessID(page);
  await assertExpectedContentPage(page);
  Assert.notEqual(childID2, childID3, "Got a new child process as expected");
  await assertHasAllowedPermission(page, "test/perm");
  await page.close();
});
