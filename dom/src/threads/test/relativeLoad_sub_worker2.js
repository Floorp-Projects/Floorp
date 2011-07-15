const importSubURL = "relativeLoad_sub_import.js";

importScripts(importSubURL);

postMessage(workerSubURL);
