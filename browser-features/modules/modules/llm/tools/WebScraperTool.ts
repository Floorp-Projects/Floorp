/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WebScraper tool for browser automation
 * Integrates with NRWebScraperParent actor for DOM operations
 */

import { BaseTool, defineTool } from "./Tool.ts";
import type { ToolResult } from "../types.ts";

/**
 * WebScraper tool definition
 */
export const WEBSCRAPER_TOOL_DEFINITION = defineTool(
  "browser_control",
  "Control the current web page via the NRWebScraper actor. Supports reading content, finding elements, clicking, filling inputs, waiting, screenshots, and highlight effects.",
  {
    type: "object",
    properties: {
      action: {
        type: "string",
        enum: [
          "read_page",
          "find_element",
          "click_element",
          "fill_input",
          "get_text",
          "scroll",
          "wait_for_element",
          "wait_for_navigation",
          "take_screenshot",
          "get_page_info",
          "highlight_element",
          "clear_highlight",
        ],
        description: "The action to perform on the web page",
      },
      selector: {
        type: "string",
        description: "CSS selector for the target element",
      },
      xpath: {
        type: "string",
        description:
          "XPath selector (currently not supported in NRWebScraper mode; use CSS selector)",
      },
      text: {
        type: "string",
        description: "Text to fill in input fields or search for",
      },
      value: {
        type: "string",
        description: "Value to set for form inputs",
      },
      url: {
        type: "string",
        description:
          "URL to navigate to (reserved; page navigation is not supported by the current NRWebScraper actor protocol)",
      },
      script: {
        type: "string",
        description:
          "JavaScript code to execute (reserved; not supported by the current NRWebScraper actor protocol)",
      },
      timeout: {
        type: "number",
        description: "Timeout in milliseconds (default: 5000)",
      },
      waitFor: {
        type: "string",
        description: "CSS selector to wait for after navigation",
      },
      highlight: {
        type: "boolean",
        description: "Whether to highlight the element before action",
      },
      scrollBehavior: {
        type: "string",
        enum: ["top", "bottom", "into-view"],
        description: "Scroll behavior for scroll action",
      },
      includeVisibility: {
        type: "boolean",
        description: "Include visibility information in results",
      },
      screenshotFormat: {
        type: "string",
        enum: ["base64", "data-url"],
        description: "Format for screenshot output",
      },
    },
    required: ["action"],
  },
);

/**
 * Type for WebScraper actor message
 */
interface WebScraperMessage {
  type: string;
  payload?: Record<string, unknown>;
}

/**
 * Type for WebScraper actor response
 */
interface WebScraperResponse {
  success: boolean;
  data?: unknown;
  error?: string;
}

/**
 * WebScraper tool implementation
 */
export class WebScraperTool extends BaseTool {
  readonly definition = WEBSCRAPER_TOOL_DEFINITION;

  /**
   * Reference to the WebScraper parent actor
   */
  private actorRef: WeakRef<WebScraperActorInterface> | null = null;

  /**
   * Set the actor reference for communication
   */
  setActorRef(actor: WebScraperActorInterface): void {
    this.actorRef = new WeakRef(actor);
  }

  private formatError(error: unknown): string {
    if (error instanceof Error && error.message) {
      return error.message;
    }
    if (typeof error === "string" && error.trim() !== "") {
      return error;
    }
    try {
      const json = JSON.stringify(error);
      if (json && json !== "{}" && json !== "[]") {
        return json;
      }
    } catch {
      // ignore
    }
    return "Unknown actor error";
  }

  private isStructuredResponse(value: unknown): value is WebScraperResponse {
    return (
      typeof value === "object" &&
      value !== null &&
      "success" in value &&
      typeof (value as { success?: unknown }).success === "boolean"
    );
  }

  private async queryActorRaw(
    actor: WebScraperActorInterface,
    type: string,
    payload?: Record<string, unknown>,
  ): Promise<unknown> {
    return await actor.sendQuery(type, payload ?? {});
  }

  private async sendToActor(
    actor: WebScraperActorInterface,
    message: WebScraperMessage,
  ): Promise<WebScraperResponse> {
    try {
      const raw = await this.queryActorRaw(actor, message.type, message.payload);
      if (this.isStructuredResponse(raw)) {
        return raw;
      }
      return { success: true, data: raw };
    } catch (error) {
      return {
        success: false,
        error: this.formatError(error),
      };
    }
  }

  private requireCssSelector(
    selector?: string,
    xpath?: string,
  ): { selector?: string; error?: string } {
    if (selector) {
      return { selector };
    }
    if (xpath) {
      return {
        error:
          "XPath selectors are not supported by the current NRWebScraper protocol. Use a CSS selector.",
      };
    }
    return { error: "Either selector or xpath is required" };
  }

  private stringifyData(data: unknown): string {
    if (typeof data === "string") {
      return data;
    }
    if (data == null) {
      return "";
    }
    try {
      return JSON.stringify(data, null, 2);
    } catch {
      return String(data);
    }
  }

  async execute(
    params: Record<string, unknown>,
    context?: { browsingContextId?: string },
  ): Promise<ToolResult> {
    const validationError = this.validateParams(params, ["action"]);
    if (validationError) {
      return this.error(validationError);
    }

    const actor = this.actorRef?.deref();
    if (!actor) {
      return this.error(
        "WebScraper actor not available. Ensure the browser is ready.",
      );
    }

    const action = params.action as string;
    const timeout = this.getNumber(params, "timeout", 5000);

    try {
      const result = await this.performAction(
        actor,
        action,
        params,
        timeout,
        context?.browsingContextId,
      );
      return result;
    } catch (e) {
      const error = e as Error;
      return this.error(`Action '${action}' failed: ${error.message}`);
    }
  }

  private performAction(
    actor: WebScraperActorInterface,
    action: string,
    params: Record<string, unknown>,
    timeout: number,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selector = this.getString(params, "selector");
    const xpath = this.getString(params, "xpath");

    switch (action) {
      case "read_page":
        return this.readPage(actor, browsingContextId);

      case "get_page_info":
        return this.getPageInfo(actor, browsingContextId);

      case "find_element":
        return this.findElement(actor, selector, xpath, browsingContextId);

      case "get_text":
        return this.getText(actor, selector, xpath, browsingContextId);

      case "click_element":
        return this.clickElement(
          actor,
          selector,
          xpath,
          params,
          browsingContextId,
        );

      case "fill_input":
        return this.fillInput(
          actor,
          selector,
          xpath,
          params,
          browsingContextId,
        );

      case "scroll":
        return this.scroll(actor, params, browsingContextId);

      case "wait_for_element":
        return this.waitForElement(
          actor,
          selector,
          xpath,
          timeout,
          browsingContextId,
        );

      case "wait_for_navigation":
        return this.waitForNavigation(actor, timeout, browsingContextId);

      case "take_screenshot":
        return this.takeScreenshot(actor, params, browsingContextId);

      case "navigate":
        return this.navigate(actor, params, browsingContextId);

      case "execute_script":
        return this.executeScript(actor, params, browsingContextId);

      case "highlight_element":
        return this.highlightElement(actor, selector, xpath, browsingContextId);

      case "clear_highlight":
        return this.clearHighlight(actor, browsingContextId);

      default:
        return Promise.resolve(this.error(`Unknown action: ${action}`));
    }
  }

  private async readPage(
    actor: WebScraperActorInterface,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const response = await this.sendToActor(actor, {
      type: "WebScraper:GetHTML",
      payload: { browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to read page");
    }
    if (response.data == null) {
      return this.error("Failed to read page");
    }

    return this.success(this.stringifyData(response.data));
  }

  private async getPageInfo(
    actor: WebScraperActorInterface,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const titleResponse = await this.sendToActor(actor, {
      type: "WebScraper:GetPageTitle",
      payload: { browsingContextId },
    });
    if (!titleResponse.success) {
      return this.error(titleResponse.error ?? "Failed to get page info");
    }

    const title =
      typeof titleResponse.data === "string" ? titleResponse.data : null;

    return this.success(
      JSON.stringify(
        {
          title,
          // The current NRWebScraper protocol does not expose URL directly.
          url: null,
        },
        null,
        2,
      ),
    );
  }

  private async findElement(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const response = await this.sendToActor(actor, {
      type: "WebScraper:GetElement",
      payload: { selector: selectorResult.selector, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Element not found");
    }
    if (typeof response.data !== "string" || response.data.length === 0) {
      return this.error("Element not found");
    }

    return this.success(response.data);
  }

  private async getText(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const response = await this.sendToActor(actor, {
      type: "WebScraper:GetElementTextContent",
      payload: { selector: selectorResult.selector, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to get text");
    }
    if (typeof response.data !== "string") {
      return this.error("Failed to get text");
    }

    return this.success(response.data);
  }

  private async clickElement(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    params?: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const highlight = params?.highlight as boolean | undefined;

    if (highlight) {
      void this.sendToActor(actor, {
        // Reuse the existing read-path inspection highlight.
        type: "WebScraper:GetElement",
        payload: { selector: selectorResult.selector, browsingContextId },
      });
    }

    const response = await this.sendToActor(actor, {
      type: "WebScraper:ClickElement",
      payload: { selector: selectorResult.selector, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to click element");
    }
    if (response.data !== true) {
      return this.error("Failed to click element");
    }

    return this.success("Element clicked successfully");
  }

  private async fillInput(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    params?: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const rawValue = params?.value ?? params?.text;
    if (
      rawValue !== undefined &&
      rawValue !== null &&
      typeof rawValue !== "string" &&
      typeof rawValue !== "number" &&
      typeof rawValue !== "boolean"
    ) {
      return this.error(
        "Input value must be a string, number, or boolean",
      );
    }

    const value = rawValue == null ? "" : String(rawValue);
    const highlight = params?.highlight as boolean | undefined;
    if (highlight) {
      void this.sendToActor(actor, {
        type: "WebScraper:GetElement",
        payload: { selector: selectorResult.selector, browsingContextId },
      });
    }

    const response = await this.sendToActor(actor, {
      type: "WebScraper:InputElement",
      payload: { selector: selectorResult.selector, value, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to fill input");
    }
    if (response.data !== true) {
      return this.error("Failed to fill input");
    }

    return this.success("Input filled successfully");
  }

  private async scroll(
    actor: WebScraperActorInterface,
    params: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const behavior = this.getString(params, "scrollBehavior", "top");
    const selector = this.getString(params, "selector");

    let response: WebScraperResponse;
    if (behavior === "into-view") {
      if (!selector) {
        return this.error("selector is required when scrollBehavior is 'into-view'");
      }
      response = await this.sendToActor(actor, {
        type: "WebScraper:ScrollToElement",
        payload: { selector, browsingContextId },
      });
    } else if (behavior === "top" || behavior === "bottom") {
      response = await this.sendToActor(actor, {
        type: "WebScraper:PressKey",
        payload: { key: behavior === "top" ? "Home" : "End", browsingContextId },
      });
    } else {
      return this.error(`Unsupported scrollBehavior: ${behavior}`);
    }

    if (!response.success) {
      return this.error(response.error ?? "Failed to scroll");
    }
    if (response.data !== true) {
      return this.error("Failed to scroll");
    }

    return this.success("Scrolled successfully");
  }

  private async waitForElement(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    timeout?: number,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const response = await this.sendToActor(actor, {
      type: "WebScraper:WaitForElement",
      payload: { selector: selectorResult.selector, timeout, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Element not found within timeout");
    }
    if (response.data !== true) {
      return this.error("Element not found within timeout");
    }

    return this.success("Element found");
  }

  private async waitForNavigation(
    actor: WebScraperActorInterface,
    timeout?: number,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const response = await this.sendToActor(actor, {
      // The existing NRWebScraper protocol exposes readiness wait, not navigation events.
      type: "WebScraper:WaitForReady",
      payload: { timeout, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Navigation timeout");
    }
    if (response.data !== true) {
      return this.error("Navigation timeout");
    }

    return this.success("Page is ready");
  }

  private async takeScreenshot(
    actor: WebScraperActorInterface,
    params: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const format = this.getString(params, "screenshotFormat", "base64");
    const selector = this.getString(params, "selector");

    const response = await this.sendToActor(actor, {
      type: selector ? "WebScraper:TakeElementScreenshot" : "WebScraper:TakeFullPageScreenshot",
      payload: selector ? { selector, browsingContextId } : { browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to take screenshot");
    }
    if (typeof response.data !== "string" || response.data.length === 0) {
      return this.error("Failed to take screenshot");
    }

    if (format === "data-url") {
      return this.success(`data:image/png;base64,${response.data}`);
    }

    return this.success(response.data);
  }

  private async navigate(
    actor: WebScraperActorInterface,
    params: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const url = this.getString(params, "url");
    if (!url) {
      return this.error("URL is required for navigation");
    }
    void browsingContextId;
    return this.error(
      "Navigation is not supported via the current NRWebScraper actor protocol. Use browser tab navigation APIs for page navigation.",
    );
  }

  private async executeScript(
    actor: WebScraperActorInterface,
    params: Record<string, unknown>,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const script = this.getString(params, "script");
    if (!script) {
      return this.error("Script is required");
    }
    void actor;
    void browsingContextId;
    void script;
    return this.error(
      "execute_script is not supported by the current NRWebScraper actor protocol.",
    );
  }

  private async highlightElement(
    actor: WebScraperActorInterface,
    selector?: string,
    xpath?: string,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const selectorResult = this.requireCssSelector(selector, xpath);
    if (selectorResult.error || !selectorResult.selector) {
      return this.error(selectorResult.error ?? "Selector is required");
    }

    const response = await this.sendToActor(actor, {
      // Reuse the existing inspection highlight path in DOMReadOperations.
      type: "WebScraper:GetElement",
      payload: { selector: selectorResult.selector, browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to highlight element");
    }
    if (typeof response.data !== "string" || response.data.length === 0) {
      return this.error("Failed to highlight element");
    }

    return this.success("Element highlighted");
  }

  private async clearHighlight(
    actor: WebScraperActorInterface,
    browsingContextId?: string,
  ): Promise<ToolResult> {
    const response = await this.sendToActor(actor, {
      type: "WebScraper:ClearEffects",
      payload: { browsingContextId },
    });

    if (!response.success) {
      return this.error(response.error ?? "Failed to clear highlight");
    }
    if (response.data !== true) {
      return this.error("Failed to clear highlight");
    }

    return this.success("Highlight cleared");
  }
}

/**
 * Interface for WebScraper actor communication
 */
export interface WebScraperActorInterface {
  sendQuery(
    type: string,
    payload: Record<string, unknown>,
  ): Promise<unknown>;
}
