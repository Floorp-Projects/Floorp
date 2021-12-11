/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const baseRelativePath = "storage/default";
  const userContextForRemoval = 2;

  const origins = [
    {
      userContextId: 1,
      baseDirName: "https+++example.com",
    },

    {
      userContextId: userContextForRemoval,
      baseDirName: "https+++example.com",
    },

    // TODO: Uncomment this once bug 1638831 is fixed.
    /*
    {
      userContextId: userContextForRemoval,
      baseDirName: "https+++example.org",
    },
    */
  ];

  function getOriginDirectory(origin) {
    return getRelativeFile(
      `${baseRelativePath}/${origin.baseDirName}^userContextId=` +
        `${origin.userContextId}`
    );
  }

  let request = init();
  await requestFinished(request);

  for (const origin of origins) {
    const directory = getOriginDirectory(origin);
    directory.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));
  }

  request = Services.qms.clearStoragesForOriginAttributesPattern(
    `{ "userContextId": ${userContextForRemoval} }`
  );
  await requestFinished(request);

  for (const origin of origins) {
    const directory = getOriginDirectory(origin);
    if (origin.userContextId === userContextForRemoval) {
      ok(!directory.exists(), "Origin directory should have been removed");
    } else {
      ok(directory.exists(), "Origin directory shouldn't have been removed");
    }
  }
}
