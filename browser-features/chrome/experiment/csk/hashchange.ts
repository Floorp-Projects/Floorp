// SPDX-License-Identifier: MPL-2.0

import { createEffect, createSignal } from "solid-js";

export const [hash, setHash] = createSignal("");

const onHashChange = (ev: Event | null) => setHash(window.location.hash);

export function initHashChange() {
  createEffect(() => {
    changeCSK();
  });
  window.addEventListener("hashchange", onHashChange);
  window.addEventListener("load", changeCSK);
  onHashChange(null);
}

export function changeCSK() {
  if (hash() === "#csk") {
    document.getElementById("cskCategory").hidden = false;
  } else {
    document.getElementById("cskCategory").hidden = true;
  }
}
