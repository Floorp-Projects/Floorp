// SPDX-License-Identifier: MPL-2.0

import { h } from "preact";
import { safeRender } from "@nora/preact-xul";
import { csk } from "./csk";
import { category } from "./csk/category";
import { initHashChange } from "./hashchange";

export default function initScripts() {
  const init = () => {
    safeRender(
      h(category, null),
      document.querySelector("#categories")!,
      document.getElementById("category-more-from-mozilla")!,
    );
    safeRender(
      h(csk, null),
      document.querySelector("#mainPrefPane")!,
    );

    initHashChange();
  };

  init();
}

if (import.meta.hot) {
  import.meta.hot.accept((m) => m?.default());
}
