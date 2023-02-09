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
      "/dom/quota/test/modules/worker/UtilsChild.js"
    );

    UtilsChild = importedUtilsChild;

    throw Error("Please switch to dynamic module import");
  } catch (e) {
    if (e.message == "Please switch to dynamic module import") {
      throw e;
    }

    importScripts("/dom/quota/test/modules/worker/UtilsChild.js");

    const { UtilsChild: importedUtilsChild } = globalThis.importUtilsChild();

    UtilsChild = importedUtilsChild;
  }
}

const Utils = {
  async shrinkStorageSize(size) {
    await ensureUtilsChild();

    const result = await UtilsChild.shrinkStorageSize(size);
    return result;
  },

  async restoreStorageSize() {
    await ensureUtilsChild();

    const result = await UtilsChild.restoreStorageSize();
    return result;
  },
};
