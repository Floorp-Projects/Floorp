/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useSignal } from "@preact/signals";
import type { ComponentChild } from "preact";
import { useEffect } from "preact/hooks";
import { getPublicContainerOptions } from "./containerUtils.ts";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

export interface SsbContainerSelectProps {
  selectedId: () => number;
  onSelect: (userContextId: number) => void;
  disabled?: () => boolean;
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

export function SsbContainerSelect(
  props: SsbContainerSelectProps,
): ComponentChild {
  const containerLabel = useSignal(
    i18next.t(props.labelKey ?? "ssb.page-action.container"),
  );

  useEffect(() => {
    addI18nObserver(() => {
      containerLabel.value = i18next.t(
        props.labelKey ?? "ssb.page-action.container",
      );
    });
  }, []);

  const options = getPublicContainerOptions();
  const selectedId = props.selectedId();
  const match = options.find((option) => option.userContextId === selectedId);
  const selectedLabel = match?.label ?? options[0]?.label ?? "";
  const isDisabled = props.disabled?.() === true;

  const handleItemCommand = (userContextId: number) => (event?: Event) => {
    event?.stopPropagation();

    const menuitem = event?.currentTarget as XULElement | undefined;
    if (menuitem) {
      hideMenuPopup(menuitem);
    }

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
        {containerLabel.value}
      </xul:label>
      <xul:button
        id="ssb-container-menu-button"
        class="ssb-container-menu-button"
        type="menu"
        label={selectedLabel}
        disabled={isDisabled ? true : undefined}
      >
        <xul:menupopup
          id="ssb-container-menupopup"
          class="in-menulist"
          {...(props.menuPopupLevel ? { level: props.menuPopupLevel } : {})}
        >
          {options.map((option) => (
            <xul:menuitem
              key={option.userContextId}
              label={option.label}
              value={String(option.userContextId)}
              closemenu="none"
              checked={option.userContextId === selectedId ? true : undefined}
              onCommand={handleItemCommand(option.userContextId)}
            />
          ))}
        </xul:menupopup>
      </xul:button>
    </xul:hbox>
  );
}
