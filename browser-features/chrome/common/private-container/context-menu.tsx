import { useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import type { ComponentChild } from "preact";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { FloorpPrivateContainer } from "./browser-private-container";

export function ContextMenu(): ComponentChild {
  const translationKey = "privateContainer.reopenInPrivateContainer";
  const label = useSignal(i18next.t(translationKey));

  useEffect(() => {
    addI18nObserver(() => {
      label.value = i18next.t(translationKey);
    });
  }, []);

  return (
    <xul:menuitem
      id="context_toggleToPrivateContainer"
      label={label.value}
      onCommand={() => {
        FloorpPrivateContainer.reopenInPrivateContainer();
      }}
    />
  );
}
