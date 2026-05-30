// SPDX-License-Identifier: MPL-2.0

import { signal } from "@preact/signals";

export function Counter() {
  const count = signal(0);
  setInterval(() => { count.value = count.value + 1; }, 1000);
  return (
    <>
      <div
        style="font-size:30px"
        onClick={() => {
          globalThis.alert("click!");
        }}
      >
        Count aa hmr: {count}
      </div>
    </>
  );
}
