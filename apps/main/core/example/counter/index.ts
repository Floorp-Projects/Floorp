import { render } from "@nora/solid-xul";
import { Counter } from "./Counter";

render(Counter, document.getElementById("appcontent")?.nextSibling, {
  hotCtx: import.meta.hot,
});

if (import.meta.hot) {
  import.meta.hot.accept();
}
