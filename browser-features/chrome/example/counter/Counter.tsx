// SPDX-License-Identifier: MPL-2.0

import { signal } from "@preact/signals";

// Module-level signal: avoids recreation on re-render.
const count = signal(0);
setInterval(() => count.value++, 1000);

export function Counter() {
  (document?.getElementById("aaa") as unknown as XULElement).style.display;
  return (
    <div
      style="font-size:30px"
      onClick={() => {
        globalThis.alert("click!");
      }}
    >
      Count aa: {count}
    </div>
  );
}
