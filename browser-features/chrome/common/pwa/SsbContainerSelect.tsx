/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createMemo, createSignal, For, type Accessor } from "solid-js";
import type { JSX } from "solid-js";
import { getPublicContainerOptions } from "./containerUtils.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export interface SsbContainerSelectProps {
  selectedId: Accessor<number>;
  onSelect: (userContextId: number) => void;
  disabled?: Accessor<boolean>;
  labelKey?: string;
  /** Use "top" for app-menu panelviews; omit for nested urlbar panels. */
  menuPopupLevel?: "top" | "parent";
}

function hideMenuPopup(menuitem: XULElement): void {
  const popup = menuitem.parentElement as unknown as XULElement & {
    hidePopup?: () => void;
  };
  popup.hidePopup?.();
}

export function SsbContainerSelect(props: SsbContainerSelectProps): JSX.Element {
  const [containerLabel, setContainerLabel] = createSignal(
    i18next.t(props.labelKey ?? "ssb.page-action.container"),
  );

  addI18nObserver(() => {
    setContainerLabel(i18next.t(props.labelKey ?? "ssb.page-action.container"));
  });

  const options = createMemo(() => getPublicContainerOptions());

  const selectedLabel = createMemo(() => {
    const selectedId = props.selectedId();
    const match = options().find((option) => option.userContextId === selectedId);
    return match?.label ?? options()[0]?.label ?? "";
  });

  const isDisabled = () => props.disabled?.() === true;

  const handleItemClick = (userContextId: number) => (event: MouseEvent) => {
    event.stopPropagation();

    const menuitem = event.currentTarget as XULElement;
    hideMenuPopup(menuitem);

    if (userContextId === props.selectedId()) {
      return;
    }

    props.onSelect(userContextId);
  };

  return (
    <xul:hbox id="ssb-container-hbox" class="ssb-container-row" align="center">
      <xul:label
        id="ssb-container-label"
        class="ssb-container-label"
      >
        {containerLabel()}
      </xul:label>
      <xul:button
        id="ssb-container-menu-button"
        class="ssb-container-menu-button"
        type="menu"
        label={selectedLabel()}
        disabled={isDisabled() ? true : undefined}
      >
        <xul:menupopup
          id="ssb-container-menupopup"
          class="in-menulist"
          {...(props.menuPopupLevel ? { level: props.menuPopupLevel } : {})}
        >
          <For each={options()}>
            {(option) => (
              <xul:menuitem
                label={option.label}
                value={String(option.userContextId)}
                closemenu="none"
                onClick={handleItemClick(option.userContextId)}
              />
            )}
          </For>
        </xul:menupopup>
      </xul:button>
    </xul:hbox>
  );
}
