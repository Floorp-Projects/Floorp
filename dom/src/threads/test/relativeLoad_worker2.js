const importURL = "relativeLoad_import.js";

importScripts(importURL);

postMessage(workerURL);
