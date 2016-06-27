/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
const importURL = "relativeLoad_import.js";

importScripts(importURL);

postMessage(workerURL);
