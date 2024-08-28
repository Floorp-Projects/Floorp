import { createSignal } from "solid-js";

export const [hash, setHash] = createSignal("");

const onHashChange = (ev: Event | null) => {
  setHash(window.location.hash);
  console.log("ChangeHash");
  if(window.location.hash === "#csk") {
    document.getElementById("cskCategory").hidden = false;
  } else {
    document.getElementById("cskCategory").hidden = true;
  }
};

export function initHashChange() {
  window.addEventListener("hashchange", onHashChange);
  window.addEventListener("load", onHashChange);
  onHashChange(null);
}
