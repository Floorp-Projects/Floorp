/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let UtilsChild;

async function ensureUtilsChild() {
  if (UtilsChild) {
    return;
  }

  try {
    const { UtilsChild: importedUtilsChild } = await import(
      "/tests/dom/quota/test/modules/worker/UtilsChild.js"
    );

    UtilsChild = importedUtilsChild;

    throw Error("Please switch to dynamic module import");
  } catch (e) {
    if (e.message == "Please switch to dynamic module import") {
      throw e;
    }

    importScripts("/tests/dom/quota/test/modules/worker/UtilsChild.js");

    const { UtilsChild: importedUtilsChild } = globalThis.importUtilsChild();

    UtilsChild = importedUtilsChild;
  }
}

const Utils = {
  async getCachedOriginUsage() {
    await ensureUtilsChild();

    const result = await UtilsChild.getCachedOriginUsage();
    return result;
  },
};
