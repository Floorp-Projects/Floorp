/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import { createRootHMR } from "@nora/preact-xul/lifetime";
import shareModeStyle from "./share-mode.css?inline";

export const shareModeEnabled = createRootHMR(
  () => signal(false),
  import.meta.hot,
);
export function ShareModeElement() {
  return (
    <>
      <xul:menuitem
        label="Toggle Share Mode"
        type="checkbox"
        id="toggle_sharemode"
        checked={shareModeEnabled.value || undefined}
        onCommand={() => (shareModeEnabled.value = !shareModeEnabled.value)}
        accesskey="S"
      />

      {shareModeEnabled.value && <style>{shareModeStyle}</style>}
    </>
  );
}
