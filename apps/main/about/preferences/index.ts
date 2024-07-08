import { render } from "@nora/solid-xul";
import { csk } from "./csk";
import { category } from "./csk/category";
import { initHashChange } from "./hashchange";

export default function initScripts() {
  const init = () => {
    console.log("init");

    const c1 = render(category, document.querySelector("#categories"), {
      marker: document.getElementById("category-more-from-mozilla")!,
      hotCtx: import.meta.hot,
    });
    const c2 = render(csk, document.querySelector(".pane-container"), {
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

  console.log("init2");

  if (document.readyState !== "loading") {
    init();
  } else {
    document.addEventListener("DOMContentLoaded", init);
  }
}

if (import.meta.hot) {
  import.meta.hot.accept((m) => m?.default());
}
