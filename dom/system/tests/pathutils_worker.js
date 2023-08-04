/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env worker */

"use strict";

/* import-globals-from /testing/mochitest/tests/SimpleTest/WorkerSimpleTest.js */
importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");

self.onmessage = async function (message) {
  let expected = message.data;
  info("ON message");
  info(JSON.stringify(expected));
  const profileDir = await PathUtils.getProfileDir();
  is(
    profileDir,
    expected.profileDir,
    "PathUtils.profileDir() in a worker should match PathUtils.profileDir on main thread"
  );

  const localProfileDir = await PathUtils.getLocalProfileDir();
  is(
    localProfileDir,
    expected.localProfileDir,
    "PathUtils.getLocalProfileDir() in a worker should match PathUtils.localProfileDir on main thread"
  );

  const tempDir = await PathUtils.getTempDir();
  is(
    tempDir,
    expected.tempDir,
    "PathUtils.getTempDir() in a worker should match PathUtils.tempDir on main thread"
  );

  finish();
};
