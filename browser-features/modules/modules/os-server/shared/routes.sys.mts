/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared route handlers for browser automation services (Scraper & Tab)
 *
 * This module provides common route registration for operations that are
 * identical between WebScraper and TabManager services.
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type {
  ErrorResponse,
  OkResponse,
} from "../_os-plugin/api-spec/types.ts";
import type { BrowserAutomationService, ScreenshotRect } from "./types.ts";

/**
 * Registers common automation routes that are shared between Scraper and Tab services.
 *
 * @param ns - The namespace builder to register routes on (already scoped to /scraper or /tabs)
 * @param getService - Function to lazily obtain the service instance
 * @param options - Configuration options for route behavior
 */
export function registerCommonAutomationRoutes(
  ns: NamespaceBuilder,
  getService: () => BrowserAutomationService,
  options: {
    /** Whether to include getElement route (Tab has it, Scraper does not expose it separately) */
    includeGetElement?: boolean;
  } = {},
): void {
  // Navigate to a URL
  ns.post<{ url: string }, OkResponse | ErrorResponse>(
    "/instances/:id/navigate",
    async (ctx: RouterContext<{ url: string }>) => {
      const json = ctx.json();
      if (!json?.url) {
        return { status: 400, body: { error: "url required" } };
      }
      const service = getService();
      await service.navigate(ctx.params.id, json.url);
      return { status: 200, body: { ok: true } };
    },
  );

  // Get current URI
  ns.get("/instances/:id/uri", async (ctx: RouterContext) => {
    const service = getService();
    const uri = await service.getURI(ctx.params.id);
    return { status: 200, body: uri != null ? { uri } : { uri: null } };
  });

  // Get page HTML
  ns.get("/instances/:id/html", async (ctx: RouterContext) => {
    const service = getService();
    const html = await service.getHTML(ctx.params.id);
    return { status: 200, body: html != null ? { html } : {} };
  });

  // Get element by selector (optional - only Tab exposes this)
  if (options.includeGetElement) {
    ns.get("/instances/:id/element", async (ctx: RouterContext) => {
      const sel = ctx.searchParams.get("selector") ?? "";
      const service = getService();
      if (!service.getElement) {
        return { status: 200, body: {} };
      }
      const element = await service.getElement(ctx.params.id, sel);
      return { status: 200, body: element != null ? { element } : {} };
    });
  }

  // Get element text by selector
  ns.get("/instances/:id/elementText", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const text = await service.getElementText(ctx.params.id, sel);
    return { status: 200, body: text != null ? { text } : {} };
  });

  // Get all matching elements (outerHTML array)
  ns.get("/instances/:id/elements", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const elems = await service.getElements(ctx.params.id, sel);
    return { status: 200, body: { elements: elems } };
  });

  // Get element by text content
  ns.get("/instances/:id/elementByText", async (ctx: RouterContext) => {
    const txt = ctx.searchParams.get("text") ?? "";
    const service = getService();
    const elem = await service.getElementByText(ctx.params.id, txt);
    return { status: 200, body: { element: elem } };
  });

  // Get element text content by selector
  ns.get("/instances/:id/elementTextContent", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const text = await service.getElementTextContent(ctx.params.id, sel);
    return { status: 200, body: text != null ? { text } : {} };
  });

  // Click element
  ns.post("/instances/:id/click", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const okClicked = await service.clickElement(ctx.params.id, sel);
    return { status: 200, body: { ok: okClicked } };
  });

  // Wait for element to appear
  ns.post("/instances/:id/waitForElement", async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      timeout?: number;
    } | null;
    const sel = json?.selector ?? "";
    const to = json?.timeout ?? 5000;
    const service = getService();
    const found = await service.waitForElement(ctx.params.id, sel, to);
    return { status: 200, body: { found } };
  });

  // Take viewport screenshot
  ns.get("/instances/:id/screenshot", async (ctx: RouterContext) => {
    const service = getService();
    const image = await service.takeScreenshot(ctx.params.id);
    return { status: 200, body: image != null ? { image } : {} };
  });

  // Take element screenshot
  ns.get("/instances/:id/elementScreenshot", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const image = await service.takeElementScreenshot(ctx.params.id, sel);
    return { status: 200, body: image != null ? { image } : {} };
  });

  // Take full page screenshot
  ns.get("/instances/:id/fullPageScreenshot", async (ctx: RouterContext) => {
    const service = getService();
    const image = await service.takeFullPageScreenshot(ctx.params.id);
    return { status: 200, body: image != null ? { image } : {} };
  });

  // Take region screenshot
  ns.post("/instances/:id/regionScreenshot", async (ctx: RouterContext) => {
    const json = ctx.json() as { rect?: ScreenshotRect } | null;
    const service = getService();
    const image = await service.takeRegionScreenshot(ctx.params.id, json?.rect);
    return { status: 200, body: image != null ? { image } : {} };
  });

  // Fill form fields
  ns.post("/instances/:id/fillForm", async (ctx: RouterContext) => {
    const json = ctx.json() as {
      formData?: { [selector: string]: string };
    } | null;
    const service = getService();
    const okFilled = await service.fillForm(
      ctx.params.id,
      json?.formData ?? {},
    );
    return { status: 200, body: { ok: okFilled } };
  });

  // Get input value
  ns.get("/instances/:id/value", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const value = await service.getValue(ctx.params.id, sel);
    return { status: 200, body: value != null ? { value } : {} };
  });

  // Submit form
  ns.post("/instances/:id/submit", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const submitted = await service.submit(ctx.params.id, sel);
    return { status: 200, body: { ok: submitted } };
  });

  // Clear input field
  ns.post("/instances/:id/clearInput", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const cleared = await service.clearInput(ctx.params.id, sel);
    return { status: 200, body: { ok: cleared } };
  });

  // Get element attribute
  ns.get("/instances/:id/attribute", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const attr = ctx.searchParams.get("name") ?? "";
    const service = getService();
    const value = await service.getAttribute(ctx.params.id, sel, attr);
    return { status: 200, body: value != null ? { value } : { value: null } };
  });

  // Check if element is visible
  ns.get("/instances/:id/isVisible", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const visible = await service.isVisible(ctx.params.id, sel);
    return { status: 200, body: { visible } };
  });

  // Check if element is enabled
  ns.get("/instances/:id/isEnabled", async (ctx: RouterContext) => {
    const sel = ctx.searchParams.get("selector") ?? "";
    const service = getService();
    const enabled = await service.isEnabled(ctx.params.id, sel);
    return { status: 200, body: { enabled } };
  });

  // Select option in a select element
  ns.post("/instances/:id/selectOption", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string; value?: string } | null;
    const sel = json?.selector ?? "";
    const value = json?.value ?? "";
    const service = getService();
    const ok = await service.selectOption(ctx.params.id, sel, value);
    return { status: 200, body: { ok } };
  });

  // Set checked state of checkbox/radio
  ns.post("/instances/:id/setChecked", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string; checked?: boolean } | null;
    const sel = json?.selector ?? "";
    const checked = json?.checked ?? false;
    const service = getService();
    const ok = await service.setChecked(ctx.params.id, sel, checked);
    return { status: 200, body: { ok } };
  });

  // Hover over element
  ns.post("/instances/:id/hover", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const ok = await service.hoverElement(ctx.params.id, sel);
    return { status: 200, body: { ok } };
  });

  // Scroll to element
  ns.post("/instances/:id/scrollTo", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const ok = await service.scrollToElement(ctx.params.id, sel);
    return { status: 200, body: { ok } };
  });

  // Get page title
  ns.get("/instances/:id/title", async (ctx: RouterContext) => {
    const service = getService();
    const title = await service.getPageTitle(ctx.params.id);
    return { status: 200, body: title != null ? { title } : { title: null } };
  });

  // Double click element
  ns.post("/instances/:id/doubleClick", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const ok = await service.doubleClick(ctx.params.id, sel);
    return { status: 200, body: { ok } };
  });

  // Right click element
  ns.post("/instances/:id/rightClick", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const ok = await service.rightClick(ctx.params.id, sel);
    return { status: 200, body: { ok } };
  });

  // Focus element
  ns.post("/instances/:id/focus", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string } | null;
    const sel = json?.selector ?? "";
    const service = getService();
    const ok = await service.focusElement(ctx.params.id, sel);
    return { status: 200, body: { ok } };
  });

  // Drag and drop
  ns.post("/instances/:id/dragAndDrop", async (ctx: RouterContext) => {
    const json = ctx.json() as {
      sourceSelector?: string;
      targetSelector?: string;
    } | null;
    const source = json?.sourceSelector ?? "";
    const target = json?.targetSelector ?? "";
    const service = getService();
    const ok = await service.dragAndDrop(ctx.params.id, source, target);
    return { status: 200, body: { ok } };
  });

  // Get cookies
  ns.get("/instances/:id/cookies", async (ctx: RouterContext) => {
    const service = getService();
    const cookies = await service.getCookies(ctx.params.id);
    return { status: 200, body: { cookies } };
  });

  // Set cookie
  ns.post<
    {
      name?: string;
      value?: string;
      domain?: string;
      path?: string;
      secure?: boolean;
      httpOnly?: boolean;
      sameSite?: "Strict" | "Lax" | "None";
      expirationDate?: number;
    },
    OkResponse | ErrorResponse
  >("/instances/:id/cookie", async (ctx: RouterContext) => {
    const json = ctx.json() as {
      name?: string;
      value?: string;
      domain?: string;
      path?: string;
      secure?: boolean;
      httpOnly?: boolean;
      sameSite?: "Strict" | "Lax" | "None";
      expirationDate?: number;
    } | null;
    if (!json?.name || !json?.value) {
      return { status: 400, body: { error: "name and value required" } };
    }
    const service = getService();
    const ok = await service.setCookie(ctx.params.id, {
      name: json.name,
      value: json.value,
      domain: json.domain,
      path: json.path,
      secure: json.secure,
      httpOnly: json.httpOnly,
      sameSite: json.sameSite,
      expirationDate: json.expirationDate,
    });
    return { status: 200, body: { ok } };
  });

  // Accept alert
  ns.post("/instances/:id/acceptAlert", async (ctx: RouterContext) => {
    const service = getService();
    const ok = await service.acceptAlert(ctx.params.id);
    return { status: 200, body: { ok } };
  });

  // Dismiss alert
  ns.post("/instances/:id/dismissAlert", async (ctx: RouterContext) => {
    const service = getService();
    const ok = await service.dismissAlert(ctx.params.id);
    return { status: 200, body: { ok } };
  });

  // Save as PDF
  ns.get("/instances/:id/pdf", async (ctx: RouterContext) => {
    const service = getService();
    const pdf = await service.saveAsPDF(ctx.params.id);
    return { status: 200, body: pdf != null ? { pdf } : {} };
  });

  // Wait for network idle
  ns.post("/instances/:id/waitForNetworkIdle", async (ctx: RouterContext) => {
    const json = ctx.json() as { timeout?: number } | null;
    const timeout = json?.timeout ?? 5000;
    const service = getService();
    const ok = await service.waitForNetworkIdle(ctx.params.id, timeout);
    return { status: 200, body: { ok } };
  });

  // Set innerHTML (for contenteditable elements)
  ns.post("/instances/:id/setInnerHTML", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string; html?: string } | null;
    const sel = json?.selector ?? "";
    const html = json?.html ?? "";
    const service = getService();
    const ok = await service.setInnerHTML(ctx.params.id, sel, html);
    return { status: 200, body: { ok } };
  });

  // Set textContent (for contenteditable elements)
  ns.post("/instances/:id/setTextContent", async (ctx: RouterContext) => {
    const json = ctx.json() as { selector?: string; text?: string } | null;
    const sel = json?.selector ?? "";
    const text = json?.text ?? "";
    const service = getService();
    const ok = await service.setTextContent(ctx.params.id, sel, text);
    return { status: 200, body: { ok } };
  });

  // Dispatch event on element
  ns.post("/instances/:id/dispatchEvent", async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      eventType?: string;
      options?: { bubbles?: boolean; cancelable?: boolean };
    } | null;
    const sel = json?.selector ?? "";
    const eventType = json?.eventType ?? "";
    const options = json?.options;
    const service = getService();
    const ok = await service.dispatchEvent(ctx.params.id, sel, eventType, options);
    return { status: 200, body: { ok } };
  });
}
