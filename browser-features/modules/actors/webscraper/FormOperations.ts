/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * FormOperations - Form-specific operations like fillForm and submit
 */

import type { FillFormOptions, WebScraperContext } from "./types.ts";
import { DOMOperations } from "./DOMOperations.ts";

const { setTimeout: timerSetTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/**
 * Helper class for form operations
 */
export class FormOperations {
  private domOps: DOMOperations;

  constructor(private context: WebScraperContext) {
    this.domOps = new DOMOperations(context);
  }

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  get document(): Document | null {
    return this.context.document;
  }

  /**
   * Get the underlying DOMOperations instance
   */
  getDOMOperations(): DOMOperations {
    return this.domOps;
  }

  /**
   * Fills multiple form fields based on a selector-value map
   */
  async fillForm(
    formData: { [selector: string]: string },
    options: FillFormOptions = {},
  ): Promise<boolean> {
    try {
      let allFilled = true;
      const selectors = Object.keys(formData);
      const fieldCount = selectors.length;
      const win = this.contentWindow;
      const doc = this.document;
      const action = "Fill";
      const highlightManager = this.domOps.getHighlightManager();
      const translationHelper = this.domOps.getTranslationHelper();
      const highlightOptions = highlightManager.getHighlightOptions(action);

      // Ensure document is minimally ready before filling
      await this.domOps.waitForReady(5000);

      // 初期情報パネルを表示
      if (fieldCount > 1 && doc) {
        await highlightManager.showInfoPanel(
          action,
          undefined,
          await translationHelper.translate("formSummary", {
            count: fieldCount,
          }),
          fieldCount,
          0,
          fieldCount,
        );
      }

      for (let i = 0; i < selectors.length; i++) {
        const selector = selectors[i];
        if (!Object.prototype.hasOwnProperty.call(formData, selector)) {
          continue;
        }

        const value = formData[selector];
        let element = this.document?.querySelector(selector) as
          | HTMLInputElement
          | HTMLTextAreaElement
          | HTMLSelectElement
          | null;

        // If not found immediately, wait briefly for it to appear
        if (!element) {
          const waited = await this.domOps.waitForElement(selector, 3000);
          if (waited) {
            element = this.document?.querySelector(selector) as
              | HTMLInputElement
              | HTMLTextAreaElement
              | HTMLSelectElement
              | null;
          }
        }

        if (element) {
          const elementInfo = await translationHelper.translate(
            "formFieldProgress",
            {
              current: i + 1,
              total: fieldCount,
              value: translationHelper.truncate(value, 30),
            },
          );
          if (fieldCount > 1 && doc) {
            await highlightManager.showInfoPanel(
              action,
              undefined,
              elementInfo,
              fieldCount,
              i + 1,
              fieldCount,
            );
          }

          await highlightManager.applyHighlight(
            element,
            highlightOptions,
            elementInfo,
            false,
          );

          let success = false;
          if (
            element instanceof (win?.HTMLSelectElement ?? HTMLSelectElement)
          ) {
            success = await this.domOps.selectOption(selector, value, {
              skipHighlight: true,
            });
          } else {
            success = await this.domOps.inputElement(selector, value, {
              ...options,
              skipHighlight: true,
            });
          }
          if (!success) {
            allFilled = false;
          }

          // 各フィールド入力後に1.2秒の遅延
          if (i < selectors.length - 1) {
            await new Promise<void>((resolve) => {
              timerSetTimeout(() => resolve(), 1200);
            });
          }
        } else {
          console.warn(
            `FormOperations: Element not found for selector: ${selector}`,
          );
          allFilled = false;
        }
      }

      // 最終的な値を確認
      if (!win) {
        return allFilled;
      }

      let finalOk = true;
      const eventDispatcher = this.domOps.getEventDispatcher();

      for (const selector of selectors) {
        const expectedValue = formData[selector];

        const element = this.document?.querySelector(selector) as
          | HTMLInputElement
          | HTMLTextAreaElement
          | HTMLSelectElement
          | null;

        if (!element) {
          finalOk = false;
          continue;
        }

        const currentValue =
          win && element instanceof win.HTMLSelectElement
            ? element.value
            : (element.value ?? "");

        if (currentValue === expectedValue) {
          continue;
        }

        try {
          if (win && element instanceof win.HTMLSelectElement) {
            const selectEl = element as HTMLSelectElement;
            const selectOptions = Array.from(
              selectEl.options,
            ) as HTMLOptionElement[];
            const targetOpt =
              selectOptions.find((opt) => opt.value === expectedValue) ?? null;
            if (targetOpt) {
              selectEl.value = targetOpt.value;
            } else {
              selectEl.value = expectedValue;
            }
          } else {
            const setter = eventDispatcher.getNativeValueSetter(
              element as HTMLInputElement | HTMLTextAreaElement,
            );
            if (setter) {
              setter(expectedValue);
            } else {
              element.value = expectedValue;
            }
          }

          eventDispatcher.dispatchInputEvents(element);
        } catch (_err) {
          try {
            if (win && element instanceof win.HTMLSelectElement) {
              const selectEl = element as HTMLSelectElement;
              const selectOptions = Array.from(
                selectEl.options,
              ) as HTMLOptionElement[];
              const targetOpt =
                selectOptions.find((opt) => opt.value === expectedValue) ??
                null;
              if (targetOpt) {
                selectEl.value = targetOpt.value;
              } else {
                selectEl.value = expectedValue;
              }
            } else {
              element.value = expectedValue;
            }
          } catch (e) {
            console.error(
              `FormOperations: Error setting value for selector ${selector}`,
              e,
            );
            finalOk = false;
          }
        }
      }

      return allFilled && finalOk;
    } catch (e) {
      console.error("FormOperations: Error filling form:", e);
      return false;
    }
  }

  /**
   * Submits a form
   */
  async submit(selector: string): Promise<boolean> {
    try {
      const root = this.document?.querySelector(selector) as Element | null;
      const form =
        (root as HTMLFormElement | null)?.tagName === "FORM"
          ? (root as HTMLFormElement)
          : (root?.closest?.("form") as HTMLFormElement | null);

      if (!form) return false;

      const formName =
        form.getAttribute("name") || form.getAttribute("id") || "form";
      const highlightManager = this.domOps.getHighlightManager();
      const translationHelper = this.domOps.getTranslationHelper();
      const elementInfo = await translationHelper.translate("submitForm", {
        name: formName,
      });
      const options = highlightManager.getHighlightOptions("Submit");

      await highlightManager.applyHighlight(root ?? form, options, elementInfo);

      try {
        const maybeRequestSubmit = (
          form as HTMLFormElement & {
            requestSubmit?: () => void;
          }
        ).requestSubmit;
        if (typeof maybeRequestSubmit === "function") {
          maybeRequestSubmit.call(form);
        } else {
          form.submit();
        }
      } catch {
        try {
          form.submit();
        } catch {
          // ignore
        }
      }
      return true;
    } catch (e) {
      console.error("FormOperations: Error submitting form:", e);
      return false;
    }
  }

  /**
   * Cleanup resources
   */
  destroy(): void {
    this.domOps.destroy();
  }
}
