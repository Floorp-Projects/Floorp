/* import-globals-from head.js */

const principalInfos = [
  { url: "http://example.com", attrs: {} },

  { url: "http://origin.test", attrs: {} },

  { url: "http://prefix.test", attrs: {} },
  { url: "http://prefix.test", attrs: { userContextId: 10 } },

  { url: "http://pattern.test", attrs: { userContextId: 15 } },
  { url: "http://pattern.test:8080", attrs: { userContextId: 15 } },
  { url: "https://pattern.test", attrs: { userContextId: 15 } },
];

const surrogate = String.fromCharCode(0xdc00);
const replacement = String.fromCharCode(0xfffd);
const beginning = "beginning";
const ending = "ending";
const complexValue = beginning + surrogate + surrogate + ending;
const corruptedValue = beginning + replacement + replacement + ending;

function enableNextGenLocalStorage() {
  info("Setting pref");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
}

function disableNextGenLocalStorage() {
  info("Setting pref");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    true
  );
}

function storeData() {
  for (let i = 0; i < principalInfos.length; i++) {
    let principalInfo = principalInfos[i];
    let principal = getPrincipal(principalInfo.url, principalInfo.attrs);

    info("Getting storage");

    let storage = getLocalStorage(principal);

    info("Adding data");

    storage.setItem("key0", "value0");
    storage.clear();
    storage.setItem("key1", "value1");
    storage.removeItem("key1");
    storage.setItem("key2", "value2");
    storage.setItem("complexKey", complexValue);

    info("Closing storage");

    storage.close();
  }
}

function exportShadowDatabase(name) {
  info("Verifying shadow database");

  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let shadowDatabase = profileDir.clone();
  shadowDatabase.append("webappsstore.sqlite");

  let exists = shadowDatabase.exists();
  ok(exists, "Shadow database does exist");

  info("Copying shadow database");

  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  shadowDatabase.copyTo(currentDir, name);
}

function importShadowDatabase(name) {
  info("Verifying shadow database");

  let currentDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  let shadowDatabase = currentDir.clone();
  shadowDatabase.append(name);

  let exists = shadowDatabase.exists();
  if (!exists) {
    return false;
  }

  info("Copying shadow database");

  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  shadowDatabase.copyTo(profileDir, "webappsstore.sqlite");

  return true;
}

function verifyData(clearedOrigins, migrated = false) {
  for (let i = 0; i < principalInfos.length; i++) {
    let principalInfo = principalInfos[i];
    let principal = getPrincipal(principalInfo.url, principalInfo.attrs);

    info("Getting storage");

    let storage = getLocalStorage(principal);

    info("Verifying data");

    if (clearedOrigins.includes(i)) {
      ok(storage.getItem("key2") == null, "Correct value");
      ok(storage.getItem("complexKey") == null, "Correct value");
    } else {
      ok(storage.getItem("key0") == null, "Correct value");
      ok(storage.getItem("key1") == null, "Correct value");
      is(storage.getItem("key2"), "value2", "Correct value");
      is(
        storage.getItem("complexKey"),
        migrated ? corruptedValue : complexValue,
        "Correct value"
      );
    }

    info("Closing storage");

    storage.close();
  }
}
