/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
const importSubURL = "relativeLoad_sub_import.js";

importScripts(importSubURL);

postMessage(workerSubURL);
