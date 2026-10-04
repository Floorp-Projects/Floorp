// SPDX-License-Identifier: MPL-2.0

import { effect, signal } from "@preact/signals";

export const hash = signal("");

const setHash = (v: string): void => {
  hash.value = v;
};

const onHashChange = (_ev: Event | null) => setHash(globalThis.location.hash);

export function initHashChange() {
  effect(() => {
    changeCSK();
  });
  globalThis.addEventListener("hashchange", onHashChange);
  globalThis.addEventListener("load", changeCSK);
  onHashChange(null);
}

export function changeCSK() {
  if (hash.value === "#csk") {
    document.getElementById("cskCategory").hidden = false;
  } else {
    document.getElementById("cskCategory").hidden = true;
  }
}
