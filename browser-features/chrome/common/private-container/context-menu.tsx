import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { FloorpPrivateContainer } from "./browser-private-container";

export function ContextMenu() {
  const translationKey = "privateContainer.reopenInPrivateContainer";
  const [label, setLabel] = createSignal(i18next.t(translationKey));

  createRootHMR(() => {
    addI18nObserver(() => {
      setLabel(i18next.t(translationKey));
    });
  }, import.meta.hot);
  return (
    <xul:menuitem
      id="context_toggleToPrivateContainer"
      label={label()}
      onCommand={() => {
        FloorpPrivateContainer.reopenInPrivateContainer();
      }}
    />
  );
}
