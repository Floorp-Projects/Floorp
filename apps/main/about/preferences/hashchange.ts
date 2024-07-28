import { createSignal } from "solid-js";

export const [hash, setHash] = createSignal("");

const onHashChange = (ev: Event | null) => setHash(location.hash);

export function initHashChange() {
  window.addEventListener("hashchange", onHashChange);
  onHashChange(null);
  setHash("#csk");
}
