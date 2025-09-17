// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";

export function Counter() {
  const [count, setCount] = createSignal(0);
  setInterval(() => setCount(count() + 1), 1000);
  (document?.getElementById("aaa") as XULElement).style.display;
  return (
    <div
      style="font-size:30px"
      onClick={() => {
        window.alert("click!");
      }}
    >
      Count aa: {count()}
    </div>
  );
}
