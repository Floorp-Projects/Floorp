// SPDX-License-Identifier: MPL-2.0

import { h, render } from "preact";
import { Counter } from "./Counter";

const container = document.getElementById("appcontent")?.nextSibling;
if (container) {
  render(h(Counter, null), container as Element);
}

if (import.meta.hot) {
  import.meta.hot.accept();
}
