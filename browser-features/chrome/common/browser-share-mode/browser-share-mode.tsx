// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";
import { Show } from "solid-js";
import shareModeStyle from "./share-mode.css?inline";

export const [shareModeEnabled, setShareModeEnabled] = createSignal(false);
export function ShareModeElement() {
  return (
    <>
      <xul:menuitem
        data-l10n-id="sharemode-menuitem"
        label="Toggle Share Mode"
        type="checkbox"
        id="toggle_sharemode"
        checked={shareModeEnabled()}
        onCommand={() => setShareModeEnabled((prev) => !prev)}
        accesskey="S"
      />

      <Show when={shareModeEnabled()}>
        <style>{shareModeStyle}</style>
      </Show>
    </>
  );
}
