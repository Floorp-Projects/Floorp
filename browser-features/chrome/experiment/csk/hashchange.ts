// SPDX-License-Identifier: MPL-2.0

import { createEffect, createSignal } from "solid-js";

export const [hash, setHash] = createSignal("");

const onHashChange = (_ev: Event | null) => setHash(globalThis.location.hash);

export function initHashChange() {
  createEffect(() => {
    changeCSK();
  });
  globalThis.addEventListener("hashchange", onHashChange);
  globalThis.addEventListener("load", changeCSK);
  onHashChange(null);
}

export function changeCSK() {
  if (hash() === "#csk") {
    document.getElementById("cskCategory").hidden = false;
  } else {
    document.getElementById("cskCategory").hidden = true;
  }
}
