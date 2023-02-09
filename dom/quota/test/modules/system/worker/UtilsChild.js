/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const _UtilsChild = {
  async shrinkStorageSize(size) {
    postMessage({
      moduleName: "UtilsParent",
      objectName: "UtilsParent",
      op: "shrinkStorageSize",
      size,
    });

    return new Promise(function(resolve) {
      addEventListener("message", async function onMessage(event) {
        removeEventListener("message", onMessage);
        const data = event.data;
        resolve(data);
      });
    });
  },

  async restoreStorageSize() {
    postMessage({
      moduleName: "UtilsParent",
      objectName: "UtilsParent",
      op: "restoreStorageSize",
    });

    return new Promise(function(resolve) {
      addEventListener("message", async function onMessage(event) {
        removeEventListener("message", onMessage);
        const data = event.data;
        resolve(data);
      });
    });
  },
};

function importUtilsChild() {
  return { UtilsChild: _UtilsChild };
}
