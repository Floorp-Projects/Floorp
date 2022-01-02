/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-env worker */
/* global workerURL */
const importURL = "relativeLoad_import.js";

importScripts(importURL);

postMessage(workerURL);
