import type {} from "solid-styled-jsx";
import { CustomShortcutKeyPage } from "./page";

import { hash } from "../hashchange";
import { initSetKey } from "./setkey";
import { Show } from "solid-js";

export function csk() {
  initSetKey();
  return (
    <section id="nora-csk-entry">
      <Show when={hash() === "#csk"}>
        <CustomShortcutKeyPage />
      </Show>
    </section>
  );
}
