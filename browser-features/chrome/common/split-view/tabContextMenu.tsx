/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { createSignal } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

const translationKeys = {
  temporarilyUnavailable: "splitview.tab-context.temporarily-unavailable",
  details: "splitview.tab-context.details",
};

export class SplitViewContextMenu {
  constructor() {
    const parentElem = document?.getElementById("tabContextMenu");
    if (!parentElem) return;

    const marker = document?.getElementById("context_closeDuplicateTabs");
    render(() => this.contextMenu(), parentElem, {
      marker: marker as XULElement,
    });
  }

  private contextMenu() {
    const [mainText, setMainText] = createSignal(
      // @ts-expect-error - Translation key not yet in type definitions
      i18next.t(translationKeys.temporarilyUnavailable) as string,
    );

    const [detailsText, setDetailsText] = createSignal(
      // @ts-expect-error - Translation key not yet in type definitions
      i18next.t(translationKeys.details) as string,
    );

    addI18nObserver(() => {
      setMainText(
        // @ts-expect-error - Translation key not yet in type definitions
        i18next.t(translationKeys.temporarilyUnavailable) as string,
      );
      // @ts-expect-error - Translation key not yet in type definitions
      setDetailsText(i18next.t(translationKeys.details) as string);
    });

    const openFeaturesPage = () => {
      try {
        window.gBrowser.addTab("https://docs.floorp.app/docs/features/split-view", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
          inBackground: false,
        });
      } catch (error) {
        console.error(
          "[SplitViewContextMenu] Failed to open features page",
          error,
        );
      }
    };

    return (
      <>
        <xul:menuseparator />
        <xul:hbox style="background-color: color-mix(in srgb, var(--arrowpanel-dimmed) 10%, transparent); margin: 2px 4px; border-radius: 3px;">
          <xul:box style="width: 2px; background-color: var(--toolbarbutton-icon-fill-attention); border-radius: 3px; margin-inline-end: 8px;" />
          <xul:vbox style="padding: 6px 8px 6px 0; flex: 1;">
            <xul:hbox align="center" style="margin-bottom: 2px;">
              <xul:image
                style="width: 14px; height: 14px; margin-inline-end: 6px; -moz-context-properties: fill; fill: var(--toolbarbutton-icon-fill-attention);"
                src="chrome://global/skin/icons/info.svg"
              />
              <xul:label>
                {mainText()}
              </xul:label>
            </xul:hbox>
            <xul:hbox
              align="center"
              pack="end"
              style="padding: 2px 4px; cursor: pointer; min-height: auto;"
              onCommand={openFeaturesPage}
              onclick={openFeaturesPage}
              onmouseover={(e: MouseEvent) => {
                const target = e.currentTarget as XULElement;
                const label = target.querySelector("label");
                if (label) {
                  label.style.setProperty("text-decoration", "underline");
                  label.style.setProperty("color", "var(--link-color)");
                }
              }}
              onmouseout={(e: MouseEvent) => {
                const target = e.currentTarget as XULElement;
                const label = target.querySelector("label");
                if (label) {
                  label.style.setProperty("text-decoration", "none");
                  label.style.setProperty(
                    "color",
                    "var(--text-color-deemphasized)",
                  );
                }
              }}
            >
              <xul:image
                style="width: 12px; height: 12px; margin-inline-end: 4px; -moz-context-properties: fill; fill: var(--link-color); opacity: 0.8;"
                src="chrome://browser/skin/link.svg"
              />
              <xul:label style="font-size: 0.9em; color: var(--text-color-deemphasized); cursor: pointer; transition: color 0.15s, text-decoration 0.15s;">
                {detailsText()}
              </xul:label>
            </xul:hbox>
          </xul:vbox>
        </xul:hbox>
        <xul:menuseparator />
      </>
    );
  }
}
