/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principalInfos = [
    { url: "http://localhost", attrs: {} },
    { url: "http://www.mozilla.org", attrs: {} },
    { url: "http://example.com", attrs: {} },
    { url: "http://example.org", attrs: { userContextId: 5 } },

    { url: "http://origin.test", attrs: {} },

    { url: "http://prefix.test", attrs: {} },
    { url: "http://prefix.test", attrs: { userContextId: 10 } },

    { url: "http://pattern.test", attrs: { userContextId: 15 } },
    { url: "http://pattern.test:8080", attrs: { userContextId: 15 } },
    { url: "https://pattern.test", attrs: { userContextId: 15 } },
  ];

  const data = {
    key: "foo",
    value: "bar",
  };

  function verifyData(clearedOrigins) {
    info("Getting storages");

    let storages = [];
    for (let i = 0; i < principalInfos.length; i++) {
      let principalInfo = principalInfos[i];
      let principal = getPrincipal(principalInfo.url, principalInfo.attrs);
      let storage = getLocalStorage(principal);
      storages.push(storage);
    }

    info("Verifying data");

    for (let i = 0; i < storages.length; i++) {
      let value = storages[i].getItem(data.key + i);
      if (clearedOrigins.includes(i)) {
        is(value, null, "Correct value");
      } else {
        is(value, data.value + i, "Correct value");
      }
    }
  }

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Stage 1 - Testing archived data migration");

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains storage.sqlite and webappsstore.sqlite. The file
  // create_db.js in the package was run locally, specifically it was
  // temporarily added to xpcshell.ini and then executed:
  // mach xpcshell-test --interactive dom/localstorage/test/unit/create_db.js
  installPackage("migration_profile");

  verifyData([]);

  info("Stage 2 - Testing archived data clearing");

  for (let type of ["origin", "prefix", "pattern"]) {
    info("Clearing");

    request = clear();
    await requestFinished(request);

    info("Installing package");

    // See the comment for the first installPackage() call.
    installPackage("migration_profile");

    let clearedOrigins = [];

    switch (type) {
      case "origin": {
        let principal = getPrincipal("http://origin.test", {});
        request = clearOrigin(principal, "default");
        await requestFinished(request);

        clearedOrigins.push(4);

        break;
      }

      case "prefix": {
        let principal = getPrincipal("http://prefix.test", {});
        request = clearOriginsByPrefix(principal, "default");
        await requestFinished(request);

        clearedOrigins.push(5, 6);

        break;
      }

      case "pattern": {
        request = clearOriginsByPattern(JSON.stringify({ userContextId: 15 }));
        await requestFinished(request);

        clearedOrigins.push(7, 8, 9);

        break;
      }

      default: {
        throw new Error("Unknown type: " + type);
      }
    }

    verifyData(clearedOrigins);
  }
}
