/**
 * Tests ProfileStorage object.
 */

/* global ProfileStorage */

"use strict";

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://formautofill/ProfileStorage.jsm");

const TEST_STORE_FILE_NAME = "test-profile.json";

const TEST_PROFILE_1 = {
  organization: "World Wide Web Consortium",
  streetAddress: "32 Vassar Street\nMIT Room 32-G524",
  addressLevel2: "Cambridge",
  addressLevel1: "MA",
  postalCode: "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

const TEST_PROFILE_2 = {
  streetAddress: "Some Address",
  country: "US",
};

const TEST_PROFILE_3 = {
  streetAddress: "Other Address",
  postalCode: "12345",
};

const TEST_PROFILE_WITH_INVALID_FIELD = {
  streetAddress: "Another Address",
  invalidField: "INVALID",
};

let prepareTestProfiles = Task.async(function* (path) {
  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  profileStorage.add(TEST_PROFILE_1);
  profileStorage.add(TEST_PROFILE_2);
  yield profileStorage._saveImmediately();
});

let do_check_profile_matches = (profileWithMeta, profile) => {
  for (let key in profile) {
    do_check_eq(profileWithMeta[key], profile[key]);
  }
};

add_task(function* test_initialize() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  do_check_eq(profileStorage._store.data.version, 1);
  do_check_eq(profileStorage._store.data.profiles.length, 0);

  let data = profileStorage._store.data;

  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  Assert.deepEqual(profileStorage._store.data, data);
});

add_task(function* test_getAll() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();

  do_check_eq(profiles.length, 2);
  do_check_profile_matches(profiles[0], TEST_PROFILE_1);
  do_check_profile_matches(profiles[1], TEST_PROFILE_2);

  // Modifying output shouldn't affect the storage.
  profiles[0].organization = "test";
  do_check_profile_matches(profileStorage.getAll()[0], TEST_PROFILE_1);
});

add_task(function* test_get() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();
  let guid = profiles[0].guid;

  let profile = profileStorage.get(guid);
  do_check_profile_matches(profile, TEST_PROFILE_1);

  // Modifying output shouldn't affect the storage.
  profile.organization = "test";
  do_check_profile_matches(profileStorage.get(guid), TEST_PROFILE_1);

  Assert.throws(() => profileStorage.get("INVALID_GUID"),
    /No matching profile\./);
});

add_task(function* test_add() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();

  do_check_eq(profiles.length, 2);

  do_check_profile_matches(profiles[0], TEST_PROFILE_1);
  do_check_profile_matches(profiles[1], TEST_PROFILE_2);

  do_check_neq(profiles[0].guid, undefined);
  do_check_neq(profiles[0].timeCreated, undefined);
  do_check_eq(profiles[0].timeLastModified, profiles[0].timeCreated);
  do_check_eq(profiles[0].timeLastUsed, 0);
  do_check_eq(profiles[0].timesUsed, 0);

  Assert.throws(() => profileStorage.add(TEST_PROFILE_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./);
});

add_task(function* test_update() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();
  let guid = profiles[1].guid;
  let timeLastModified = profiles[1].timeLastModified;

  do_check_neq(profiles[1].country, undefined);

  profileStorage.update(guid, TEST_PROFILE_3);
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profile = profileStorage.get(guid);

  do_check_eq(profile.country, undefined);
  do_check_neq(profile.timeLastModified, timeLastModified);
  do_check_profile_matches(profile, TEST_PROFILE_3);

  Assert.throws(
    () => profileStorage.update("INVALID_GUID", TEST_PROFILE_3),
    /No matching profile\./
  );

  Assert.throws(
    () => profileStorage.update(guid, TEST_PROFILE_WITH_INVALID_FIELD),
    /"invalidField" is not a valid field\./
  );
});

add_task(function* test_notifyUsed() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();
  let guid = profiles[1].guid;
  let timeLastUsed = profiles[1].timeLastUsed;
  let timesUsed = profiles[1].timesUsed;

  profileStorage.notifyUsed(guid);
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profile = profileStorage.get(guid);

  do_check_eq(profile.timesUsed, timesUsed + 1);
  do_check_neq(profile.timeLastUsed, timeLastUsed);

  Assert.throws(() => profileStorage.notifyUsed("INVALID_GUID"),
    /No matching profile\./);
});

add_task(function* test_remove() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;
  yield prepareTestProfiles(path);

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();
  let guid = profiles[1].guid;

  do_check_eq(profiles.length, 2);

  profileStorage.remove(guid);
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  profiles = profileStorage.getAll();

  do_check_eq(profiles.length, 1);

  Assert.throws(() => profileStorage.get(guid), /No matching profile\./);
});
