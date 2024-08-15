import { render } from "@nora/solid-xul";
import { csk } from "./csk";
import { category } from "./csk/category";
import { initHashChange } from "./hashchange";

export default function initScripts() {
  const init = () => {
    render(category, document.querySelector("#categories"), {
      marker: document.getElementById("category-more-from-mozilla")!,
      hotCtx: import.meta.hot,
    });
    render(csk, document.querySelector(".pane-container"), {
      hotCtx: import.meta.hot,
    });

    initHashChange();
    switch (location.hash) {
      case "#csk": {
        document.getElementById("category-csk")?.click();
        break;
      }
    }
  };

  document.addEventListener("DOMContentLoaded", init);
}

if (import.meta.hot) {
  import.meta.hot.accept((m) => m?.default());
}
