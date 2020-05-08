/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const origins = [
    {
      origin: "https://example.com",
      attribute: { userContextId: 1 },
      path: "https+++example.com^userContextId=1",
    },
    {
      origin: "https://example.com",
      attribute: { userContextId: 2 },
      path: "https+++example.com^userContextId=2",
    },
  ];

  let request;
  for (let origin of origins) {
    request = initStorageAndOrigin(
      getPrincipal(origin.origin, origin.attribute),
      "default"
    );
    await requestFinished(request);
  }

  const path = "storage/default/";
  for (let origin of origins) {
    let originDir = getRelativeFile(path + origin.path);
    ok(originDir.exists(), "Origin directory should have been created");
  }

  const removingId = 2;
  await new Promise(function(aResolve) {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      {
        userContextId: removingId,
      },
      aResolve
    );
  });

  for (let origin of origins) {
    let originDir = getRelativeFile(path + origin.path);
    if (origin.attribute.userContextId === removingId) {
      ok(!originDir.exists(), "Origin directory should have been removed");
    } else {
      ok(originDir.exists(), "Origin directory shouldn't have been removed");
    }
  }
}
