/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export const UtilsChild = {
  async getCachedOriginUsage() {
    postMessage({
      moduleName: "UtilsParent",
      objectName: "UtilsParent",
      op: "getCachedOriginUsage",
    });

    return new Promise(function (resolve) {
      addEventListener("message", async function onMessage(event) {
        removeEventListener("message", onMessage);
        const data = event.data;
        resolve(data);
      });
    });
  },
};
