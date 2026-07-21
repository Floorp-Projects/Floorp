// SPDX-License-Identifier: MPL-2.0

import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { createSignal, For, Show } from "solid-js";
import type { SilverPassManager } from "./silver-pass-manager.ts";
import type { SilverPassI18nKey } from "./types.ts";

type SilverPassPanelProps = {
  manager: SilverPassManager;
};

export function SilverPassPanel(props: SilverPassPanelProps) {
  const [localeVersion, setLocaleVersion] = createSignal(0);
  addI18nObserver(() => setLocaleVersion((version) => version + 1));

  const translate = (key: SilverPassI18nKey): string => {
    localeVersion();
    return i18next.t(key);
  };

  const handleHighlight = async (): Promise<void> => {
    if (!await props.manager.highlightNextStep()) return;
    const panel = document?.getElementById(
      "silver-pass-panel",
    ) as XULPopupElement | null;
    panel?.hidePopup();
  };

  return (
    <xul:panel
      id="silver-pass-panel"
      type="arrow"
      position="bottomright topright"
      noautofocus
      role="dialog"
      aria-labelledby="silver-pass-panel-title"
      data-silver-pass-state={props.manager.state().status}
      onPopupShowing={() => void props.manager.analyzeCurrentPage()}
    >
      <xul:vbox id="silver-pass-panel-content">
        <xul:hbox class="silver-pass-header" align="center">
          <xul:vbox flex="1">
            <xul:label id="silver-pass-panel-title" class="silver-pass-title">
              {translate("silverPassDemo.panelTitle")}
            </xul:label>
            <xul:description class="silver-pass-subtitle">
              {translate("silverPassDemo.definitionsDisclaimer")}
            </xul:description>
          </xul:vbox>
        </xul:hbox>

        <Show when={props.manager.state().status === "loading"}>
          <xul:hbox
            class="silver-pass-message silver-pass-loading"
            align="center"
            role="status"
          >
            <xul:label class="silver-pass-loading-dot" aria-hidden="true" />
            <xul:description data-silver-pass-field="message">
              {translate("silverPassDemo.loading")}
            </xul:description>
          </xul:hbox>
        </Show>

        <Show
          when={props.manager.state().status === "unsupported" ||
            props.manager.state().status === "error"}
        >
          <xul:description
            class={`silver-pass-message silver-pass-message--${props.manager.state().status}`}
            data-silver-pass-field="message"
            role="status"
          >
            {translate(
              props.manager.state().messageKey ??
                "silverPassDemo.errorGeneric",
            )}
          </xul:description>
        </Show>

        <Show
          when={props.manager.state().status === "ready" &&
            props.manager.state().analysis !== null}
        >
          <xul:vbox class="silver-pass-body">
            <xul:vbox class="silver-pass-section">
              <xul:label class="silver-pass-section-title">
                {translate("silverPassDemo.sectionPagePurpose")}
              </xul:label>
              <xul:description data-silver-pass-field="purpose">
                {translate(props.manager.state().analysis!.purposeKey)}
              </xul:description>
            </xul:vbox>

            <xul:vbox class="silver-pass-section silver-pass-section--primary">
              <xul:label class="silver-pass-section-title">
                {translate("silverPassDemo.sectionNextStep")}
              </xul:label>
              <xul:description data-silver-pass-field="next-step">
                {translate(props.manager.state().analysis!.nextStepKey)}
              </xul:description>
              <xul:label class="silver-pass-reason-label">
                {translate("silverPassDemo.sectionReason")}
              </xul:label>
              <xul:description data-silver-pass-field="reason">
                {translate(props.manager.state().analysis!.reasonKey)}
              </xul:description>
              <xul:button
                class="silver-pass-highlight-button"
                data-silver-pass-action="highlight"
                label={translate("silverPassDemo.highlightButton")}
                onCommand={() => void handleHighlight()}
              />
            </xul:vbox>

            <xul:vbox class="silver-pass-section">
              <xul:label class="silver-pass-section-title">
                {translate("silverPassDemo.sectionDifficultTerms")}
              </xul:label>
              <xul:vbox class="silver-pass-terms">
                <For each={props.manager.state().analysis?.terms ?? []}>
                  {(term) => (
                    <xul:vbox
                      class="silver-pass-term"
                      data-silver-pass-term={term.id}
                    >
                      <xul:label class="silver-pass-term-label">
                        {translate(term.labelKey)}
                      </xul:label>
                      <xul:description class="silver-pass-term-explanation">
                        {translate(term.explanationKey)}
                      </xul:description>
                    </xul:vbox>
                  )}
                </For>
              </xul:vbox>
            </xul:vbox>
          </xul:vbox>
        </Show>
      </xul:vbox>
    </xul:panel>
  );
}
