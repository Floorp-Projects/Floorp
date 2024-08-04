import { render } from "@nora/solid-xul";
import { IconBar } from "./IconBar";
import Sortable from "sortablejs";

export function initSidebar() {
  render(
    () => (
      <section id="@nora:tmp:sidebar:base">
        <IconBar />
      </section>
    ),
    document.getElementById("browser"),
  );
}
