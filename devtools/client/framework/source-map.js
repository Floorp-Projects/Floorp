// @flow

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId
} = require("./source-map-util");

function workerTask(worker, method) {
  return function(...args: any) {
    return new Promise((resolve, reject) => {
      const id = msgId++;
      worker.postMessage({ id, method, args });

      const listener = ({ data: result }) => {
        if (result.id !== id) {
          return;
        }

        worker.removeEventListener("message", listener);
        if (result.error) {
          reject(result.error);
        } else {
          resolve(result.response);
        }
      };

      worker.addEventListener("message", listener);
    });
  };
}

let sourceMapWorker;
function restartWorker() {
  if (sourceMapWorker) {
    sourceMapWorker.terminate();
  }
  sourceMapWorker = new Worker(
    "resource://devtools/client/framework/source-map-worker.js"
  );

  if (Services.prefs.getBoolPref("devtools.debugger.client-source-maps-enabled")) {
    sourceMapWorker.postMessage({ id: 0, method: "enableSourceMaps" });
  }
}
restartWorker();

function destroyWorker() {
  if (sourceMapWorker) {
    sourceMapWorker.terminate();
    sourceMapWorker = null;
  }
}

function shouldSourceMap() {
  return Services.prefs.getBoolPref("devtools.debugger.client-source-maps-enabled");
}

const getOriginalURLs = workerTask(sourceMapWorker, "getOriginalURLs");
const getGeneratedLocation = workerTask(sourceMapWorker,
                                        "getGeneratedLocation");
const getOriginalLocation = workerTask(sourceMapWorker,
                                       "getOriginalLocation");
const getOriginalSourceText = workerTask(sourceMapWorker,
                                         "getOriginalSourceText");
const applySourceMap = workerTask(sourceMapWorker, "applySourceMap");
const clearSourceMaps = workerTask(sourceMapWorker, "clearSourceMaps");

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,

  getOriginalURLs,
  getGeneratedLocation,
  getOriginalLocation,
  getOriginalSourceText,
  applySourceMap,
  clearSourceMaps,
  destroyWorker,
  shouldSourceMap
};
