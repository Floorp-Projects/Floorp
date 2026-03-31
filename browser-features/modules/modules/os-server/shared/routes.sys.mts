/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared route handlers for browser automation services (Scraper & Tab)
 *
 * This module provides common route registration for operations that are
 * identical between WebScraper and TabManager services.
 *
 * Fingerprint-based targeting is preferred, with selector fallback kept for
 * backward compatibility with existing clients.
 */

import type {
  NamespaceBuilder,
  Context as RouterContext,
} from "../router.sys.mts";
import type {
  ErrorResponse,
  OkResponse,
} from "../_os-plugin/api-spec/types.ts";
import type {
  BrowserAutomationService,
  ElementResponse,
  ScreenshotRect,
  TextResponse,
  ValueResponse,
  WaitForElementState,
} from "./types.ts";

const FINGERPRINT_REGEX = /^[a-z0-9]{8}([a-z0-9]{8})?$/;
const MAX_TIMEOUT_MS = 60_000;
const ALLOWED_URL_SCHEMES = new Set(["http:", "https:"]);
const ALLOWED_EXACT_URLS = new Set(["about:blank"]);

/**
 * Clamp a timeout value to a safe range [0, MAX_TIMEOUT_MS].
 */
function clampTimeout(value: number | undefined, fallback: number): number {
  const v = typeof value === "number" ? value : fallback;
  return Math.min(Math.max(v, 0), MAX_TIMEOUT_MS);
}

/**
 * Wraps a route handler with try/catch to prevent service exceptions
 * from propagating as unhandled rejections.
 */
export function safeRoute<F extends (...args: never[]) => Promise<unknown>>(handler: F): F {
  return (async (...args: unknown[]) => {
    try {
      return await handler(...(args as Parameters<F>));
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      console.error("Route handler error:", msg);
      // Map "not found" service errors to 404
      if (/(?:instance|browser)\s+not\s+found/i.test(msg)) {
        return { status: 404, body: { error: msg } };
      }
      return { status: 500, body: { error: `Internal server error: ${msg}` } };
    }
  }) as unknown as F;
}

type FingerprintResult =
  | { ok: true; selector: string }
  | { ok: false; error: { status: number; body: { error: string } } };

/**
 * Resolves a CSS selector from an element fingerprint.
 * Validates format and resolves via the service's fingerprint map.
 *
 * @param service - The browser automation service
 * @param instanceId - The instance ID
 * @param fingerprint - Element fingerprint (8 or 16 lowercase alphanumeric chars)
 * @returns Resolution result with selector or error
 */
async function resolveFingerprint(
  service: BrowserAutomationService,
  instanceId: string,
  fingerprint: string | null | undefined,
): Promise<FingerprintResult> {
  if (!fingerprint) {
    return {
      ok: false,
      error: { status: 400, body: { error: "fingerprint required" } },
    };
  }
  if (!FINGERPRINT_REGEX.test(fingerprint)) {
    return {
      ok: false,
      error: {
        status: 400,
        body: {
          error:
            "Invalid fingerprint format. Expected 8 or 16 lowercase alphanumeric characters.",
        },
      },
    };
  }
  if (!service.resolveFingerprint) {
    return {
      ok: false,
      error: {
        status: 501,
        body: { error: "fingerprint resolution not supported" },
      },
    };
  }
  const selector = await service.resolveFingerprint(instanceId, fingerprint);
  if (!selector) {
    return {
      ok: false,
      error: { status: 404, body: { error: "fingerprint not found" } },
    };
  }
  return { ok: true, selector };
}

/**
 * Resolves a CSS selector from either direct selector input or fingerprint.
 * Priority: selector > fingerprint
 */
async function resolveSelectorOrFingerprint(
  service: BrowserAutomationService,
  instanceId: string,
  selector: string | null | undefined,
  fingerprint: string | null | undefined,
): Promise<FingerprintResult> {
  const normalizedSelector = selector?.trim();
  if (normalizedSelector) {
    return { ok: true, selector: normalizedSelector };
  }

  if (!fingerprint) {
    return {
      ok: false,
      error: {
        status: 400,
        body: { error: "selector or fingerprint required" },
      },
    };
  }

  return await resolveFingerprint(service, instanceId, fingerprint);
}

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
  // Navigate to a URL (only http/https allowed)
  ns.post<{ url: string }, OkResponse | ErrorResponse>(
    "/instances/:id/navigate",
    safeRoute(async (ctx: RouterContext<{ url: string }>) => {
      const json = ctx.json();
      if (!json?.url) {
        return { status: 400, body: { error: "url required" } };
      }
      if (!ALLOWED_EXACT_URLS.has(json.url)) {
        try {
          const parsed = new URL(json.url);
          if (!ALLOWED_URL_SCHEMES.has(parsed.protocol)) {
            return {
              status: 400,
              body: { error: `URL scheme '${parsed.protocol}' not allowed. Only http/https are permitted.` },
            };
          }
        } catch {
          return { status: 400, body: { error: "Invalid URL" } };
        }
      }
      const service = getService();
      await service.navigate(ctx.params.id, json.url);
      return { status: 200, body: { ok: true } };
    }),
  );

  // Get current URI
  ns.get("/instances/:id/uri", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const uri = await service.getURI(ctx.params.id);
    return { status: 200, body: uri != null ? { uri } : { uri: null } };
  }));

  // Get page HTML (optional scoped by selector query param)
  ns.get("/instances/:id/html", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const selector = ctx.searchParams.get("selector") || undefined;
    const html = await service.getHTML(ctx.params.id, selector ? { selector } : undefined);
    return { status: 200, body: { html } };
  }));

  // Get visible text content (as Markdown with element fingerprints) - legacy GET
  ns.get("/instances/:id/text", safeRoute(async (ctx: RouterContext) => {
    const includeSelectorMap =
      ctx.searchParams.get("includeSelectorMap") === "true";
    const service = getService();
    const text = await service.getText(ctx.params.id, includeSelectorMap);
    return { status: 200, body: text != null ? { text } : {} };
  }));

  // Get text content with full options (mode, selector, etc.)
  ns.post("/instances/:id/text", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const body = ctx.json() as Record<string, unknown> | null;
    const rawMode = body?.mode as string | undefined;
    const mode = (rawMode === "scoped" || rawMode === "visible") ? rawMode : "full";
    const options = {
      mode: mode as "full" | "scoped" | "visible",
      selector: body?.selector as string | undefined,
      viewportMargin: typeof body?.viewportMargin === "number" ? body.viewportMargin : undefined,
      enableFingerprints: typeof body?.enableFingerprints === "boolean" ? body.enableFingerprints : undefined,
      includeSelectorMap: typeof body?.includeSelectorMap === "boolean" ? body.includeSelectorMap : undefined,
    };
    const text = await service.getText(ctx.params.id, options);
    return { status: 200, body: text != null ? { text } : {} };
  }));

  // Get accessibility tree
  ns.get("/instances/:id/ax-tree", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    if (!service.getAccessibilityTree) {
      return { status: 501, body: { error: "getAccessibilityTree not supported" } as Record<string, unknown> };
    }
    const interestingOnly = ctx.searchParams.get("interestingOnly") !== "false";
    const root = ctx.searchParams.get("root") || undefined;
    const tree = await service.getAccessibilityTree(ctx.params.id, {
      interestingOnly,
      root,
    });
    return { status: 200, body: (tree != null ? { tree } : {}) as Record<string, unknown> };
  }));

  // Get article (Readability extraction)
  ns.get("/instances/:id/article", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    if (!service.getArticle) {
      return { status: 501, body: { error: "getArticle not supported" } };
    }
    const article = await service.getArticle(ctx.params.id);
    return { status: 200, body: article ?? { error: "Could not extract article" } };
  }));

  // Get element by selector or fingerprint (optional - only Tab exposes this)
  if (options.includeGetElement) {
    ns.get<unknown, ElementResponse>(
      "/instances/:id/element",
      safeRoute(async (ctx: RouterContext) => {
        const selector = ctx.searchParams.get("selector");
        const fingerprint = ctx.searchParams.get("fingerprint");
        const service = getService();
        const resolved = await resolveSelectorOrFingerprint(
          service,
          ctx.params.id,
          selector,
          fingerprint,
        );
        if (!resolved.ok) return resolved.error;
        if (!service.getElement) {
          return { status: 200, body: {} };
        }
        const element = await service.getElement(
          ctx.params.id,
          resolved.selector,
        );
        return { status: 200, body: element != null ? { element } : {} };
      }),
    );
  }

  // Get element text by selector or fingerprint
  ns.get<unknown, TextResponse>(
    "/instances/:id/elementText",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const text = await service.getElementText(
        ctx.params.id,
        resolved.selector,
      );
      return { status: 200, body: text != null ? { text } : {} };
    }),
  );

  // Get all matching elements (outerHTML array) by selector or fingerprint
  ns.get<unknown, { elements?: string[]; error?: string }>(
    "/instances/:id/elements",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const elems = await service.getElements(ctx.params.id, resolved.selector);
      return { status: 200, body: { elements: elems } };
    }),
  );

  // Get element by text content
  ns.get<unknown, ElementResponse>(
    "/instances/:id/elementByText",
    safeRoute(async (ctx: RouterContext) => {
      const txt = ctx.searchParams.get("text") ?? "";
      if (!txt) {
        return { status: 400, body: { error: "text parameter required" } };
      }
      const service = getService();
      const elem = await service.getElementByText(ctx.params.id, txt);
      return { status: 200, body: elem != null ? { element: elem } : {} };
    }),
  );

  // Get element text content by selector or fingerprint
  ns.get<unknown, TextResponse>(
    "/instances/:id/elementTextContent",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const text = await service.getElementTextContent(
        ctx.params.id,
        resolved.selector,
      );
      return { status: 200, body: text != null ? { text } : {} };
    }),
  );

  // Resolve fingerprint to CSS selector
  ns.get<{ selector?: string; error?: string }>(
    "/instances/:id/resolveFingerprint",
    safeRoute(async (ctx: RouterContext) => {
      const fingerprint = ctx.searchParams.get("fingerprint") ?? "";

      // Validate fingerprint format (8 or 16 alphanumeric lowercase chars)
      if (!fingerprint || !FINGERPRINT_REGEX.test(fingerprint)) {
        return {
          status: 400,
          body: {
            error:
              "Invalid fingerprint format. Expected 8 or 16 lowercase alphanumeric characters.",
          },
        };
      }

      const service = getService();
      if (!service.resolveFingerprint) {
        return {
          status: 501,
          body: { error: "fingerprint resolution not supported" },
        };
      }
      const selector = await service.resolveFingerprint(
        ctx.params.id,
        fingerprint,
      );
      if (selector == null) {
        return { status: 404, body: { error: "fingerprint not found" } };
      }
      return { status: 200, body: { selector } };
    }),
  );

  // Clear all visual effects (highlights, overlays, info panels)
  ns.post<unknown, { ok?: boolean; error?: string }>(
    "/instances/:id/clearEffects",
    safeRoute(async (ctx: RouterContext) => {
      const service = getService();
      if (!service.clearEffects) {
        return { status: 501, body: { error: "clearEffects not supported" } };
      }
      const ok = await service.clearEffects(ctx.params.id);
      return { status: 200, body: { ok } };
    }),
  );

  // Click element (selector or fingerprint, with optional click options)
  ns.post<{ fingerprint?: string }, { ok: boolean } | ErrorResponse>(
    "/instances/:id/click",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
        button?: "left" | "right" | "middle";
        clickCount?: number;
        force?: boolean;
        stabilityTimeout?: number;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const okClicked = await service.clickElement(
        ctx.params.id,
        resolved.selector,
        {
          button: json?.button,
          clickCount: json?.clickCount,
          force: json?.force,
          stabilityTimeout: json?.stabilityTimeout,
        },
      );
      return { status: 200, body: { ok: okClicked ?? false } };
    }),
  );

  // Wait for element to appear (selector or fingerprint)
  ns.post<
    { fingerprint: string; timeout?: number; state?: WaitForElementState },
    { ok: boolean; found: boolean } | ErrorResponse
  >("/instances/:id/waitForElement", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint: string;
      timeout?: number;
      state?: WaitForElementState;
    } | null;
    const state = json?.state ?? "attached";

    // Fingerprint resolution requires the element to already exist in the DOM.
    // Waiting for "attached" state with a fingerprint is contradictory since
    // we cannot resolve a fingerprint for an element that doesn't exist yet.
    if (!json?.selector && json?.fingerprint && state === "attached") {
      return {
        status: 400,
        body: {
          error:
            'waitForElement with fingerprint does not support state "attached". ' +
            "Fingerprints can only be resolved for elements already in the DOM. " +
            'Use a CSS selector for "attached" state, or use "visible"/"hidden"/"detached" with a fingerprint.',
        },
      };
    }

    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const to = clampTimeout(json?.timeout, 5000);
    const found = await service.waitForElement(
      ctx.params.id,
      resolved.selector,
      to,
      state,
    );
    const result = found ?? false;
    return { status: 200, body: { ok: result, found: result } };
  }));

  // Take viewport screenshot
  ns.get("/instances/:id/screenshot", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const image = await service.takeScreenshot(ctx.params.id);
    return { status: 200, body: { image } };
  }));

  // Take element screenshot by selector or fingerprint
  ns.get<unknown, { image?: string; error?: string }>(
    "/instances/:id/elementScreenshot",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const image = await service.takeElementScreenshot(
        ctx.params.id,
        resolved.selector,
      );
      return { status: 200, body: image != null ? { image } : {} };
    }),
  );

  // Take full page screenshot
  ns.get("/instances/:id/fullPageScreenshot", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const image = await service.takeFullPageScreenshot(ctx.params.id);
    return { status: 200, body: { image } };
  }));

  // Take region screenshot
  ns.post("/instances/:id/regionScreenshot", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as { rect?: ScreenshotRect } | null;
    const service = getService();
    const image = await service.takeRegionScreenshot(ctx.params.id, json?.rect);
    return { status: 200, body: { image } };
  }));

  // Fill form fields (selector keys, or fingerprint keys when resolvable)
  ns.post<
    {
      formData?: Record<string, string>;
      typingMode?: boolean;
      typingDelayMs?: number;
    },
    OkResponse | ErrorResponse
  >("/instances/:id/fillForm", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      formData?: Record<string, string>;
      typingMode?: boolean;
      typingDelayMs?: number;
    } | null;

    const service = getService();
    const formData = json?.formData ?? {};

    // Resolve fingerprint-like keys to selectors when possible.
    // Fallback to selector key for backward compatibility.
    const resolvedFormData: Record<string, string> = {};
    for (const [selectorOrFingerprint, value] of Object.entries(formData)) {
      const key = selectorOrFingerprint.trim();
      if (!key) {
        return {
          status: 400,
          body: { error: "selector or fingerprint required" },
        };
      }

      if (FINGERPRINT_REGEX.test(key)) {
        if (!service.resolveFingerprint) {
          return { status: 501, body: { error: "fingerprint resolution not supported" } };
        }
        const resolvedSelector = await service.resolveFingerprint(
          ctx.params.id,
          key,
        );
        if (!resolvedSelector) {
          return { status: 404, body: { error: `Fingerprint '${key}' not found` } };
        }
        resolvedFormData[resolvedSelector] = value;
        continue;
      }

      resolvedFormData[key] = value;
    }

    const okFilled = await service.fillForm(ctx.params.id, resolvedFormData, {
      typingMode: json?.typingMode,
      typingDelayMs: json?.typingDelayMs,
    });
    return { status: 200, body: { ok: okFilled ?? false } };
  }));

  // Get input value by selector or fingerprint
  ns.get<unknown, ValueResponse>(
    "/instances/:id/value",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const value = await service.getValue(ctx.params.id, resolved.selector);
      return { status: 200, body: value != null ? { value } : {} };
    }),
  );

  // Submit form (selector or fingerprint)
  ns.post<{ fingerprint?: string }, { ok: boolean } | ErrorResponse>(
    "/instances/:id/submit",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const submitted = await service.submit(ctx.params.id, resolved.selector);
      return { status: 200, body: { ok: submitted ?? false } };
    }),
  );

  // Clear input field (selector or fingerprint)
  ns.post<{ fingerprint?: string }, { ok: boolean } | ErrorResponse>(
    "/instances/:id/clearInput",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const cleared = await service.clearInput(
        ctx.params.id,
        resolved.selector,
      );
      return { status: 200, body: { ok: cleared ?? false } };
    }),
  );

  // Get element attribute by selector or fingerprint
  ns.get<unknown, { value?: string | null; error?: string }>(
    "/instances/:id/attribute",
    safeRoute(async (ctx: RouterContext) => {
      const attr = ctx.searchParams.get("name") ?? "";
      if (!attr) {
        return { status: 400, body: { error: "name parameter required" } };
      }
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const value = await service.getAttribute(
        ctx.params.id,
        resolved.selector,
        attr,
      );
      return { status: 200, body: value != null ? { value } : { value: null } };
    }),
  );

  // Check if element is visible by selector or fingerprint
  ns.get<unknown, { visible?: boolean; error?: string }>(
    "/instances/:id/isVisible",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const visible = await service.isVisible(ctx.params.id, resolved.selector);
      return { status: 200, body: { visible } };
    }),
  );

  // Check if element is enabled by selector or fingerprint
  ns.get<unknown, { enabled?: boolean; error?: string }>(
    "/instances/:id/isEnabled",
    safeRoute(async (ctx: RouterContext) => {
      const selector = ctx.searchParams.get("selector");
      const fingerprint = ctx.searchParams.get("fingerprint");
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        selector,
        fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const enabled = await service.isEnabled(ctx.params.id, resolved.selector);
      return { status: 200, body: { enabled } };
    }),
  );

  // Select option in a select element (selector or fingerprint)
  ns.post<
    { fingerprint: string; value?: string },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/selectOption", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint?: string;
      value?: string;
    } | null;
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const value = json?.value ?? "";
    const ok = await service.selectOption(
      ctx.params.id,
      resolved.selector,
      value,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Set checked state of checkbox/radio (selector or fingerprint)
  ns.post<
    { fingerprint: string; checked?: boolean },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/setChecked", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint?: string;
      checked?: boolean;
    } | null;
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const checked = json?.checked ?? false;
    const ok = await service.setChecked(
      ctx.params.id,
      resolved.selector,
      checked,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Hover over element (selector or fingerprint)
  ns.post<{ fingerprint?: string }, { ok: boolean } | ErrorResponse>(
    "/instances/:id/hover",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const ok = await service.hoverElement(ctx.params.id, resolved.selector);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Scroll to element (selector or fingerprint)
  ns.post<{ fingerprint?: string }, { ok: boolean } | ErrorResponse>(
    "/instances/:id/scrollTo",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      const ok = await service.scrollToElement(
        ctx.params.id,
        resolved.selector,
      );
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Get page title
  ns.get("/instances/:id/title", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const title = await service.getPageTitle(ctx.params.id);
    return { status: 200, body: title != null ? { title } : { title: null } };
  }));

  // Double click element (selector or fingerprint)
  ns.post<{ fingerprint: string }, { ok?: boolean; error?: string }>(
    "/instances/:id/doubleClick",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      if (!service.doubleClick) {
        return { status: 501, body: { error: "doubleClick not supported" } };
      }
      const ok = await service.doubleClick(ctx.params.id, resolved.selector);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Right click element (selector or fingerprint)
  ns.post<{ fingerprint: string }, { ok?: boolean; error?: string }>(
    "/instances/:id/rightClick",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      if (!service.rightClick) {
        return { status: 501, body: { error: "rightClick not supported" } };
      }
      const ok = await service.rightClick(ctx.params.id, resolved.selector);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Focus element (selector or fingerprint)
  ns.post<{ fingerprint: string }, { ok?: boolean; error?: string }>(
    "/instances/:id/focus",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;
      if (!service.focusElement) {
        return { status: 501, body: { error: "focusElement not supported" } };
      }
      const ok = await service.focusElement(ctx.params.id, resolved.selector);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Drag and drop (selector or fingerprint for both source and target)
  ns.post<
    { sourceFingerprint: string; targetFingerprint: string },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/dragAndDrop", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      sourceSelector?: string;
      sourceFingerprint?: string;
      targetSelector?: string;
      targetFingerprint?: string;
    } | null;
    const service = getService();
    const source = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.sourceSelector,
      json?.sourceFingerprint,
    );
    if (!source.ok) return source.error;
    const target = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.targetSelector,
      json?.targetFingerprint,
    );
    if (!target.ok) return target.error;
    const ok = await service.dragAndDrop(
      ctx.params.id,
      source.selector,
      target.selector,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Get cookies
  ns.get("/instances/:id/cookies", safeRoute(async (ctx: RouterContext) => {
    const service = getService();
    const cookies = await service.getCookies(ctx.params.id);
    return { status: 200, body: { cookies } };
  }));

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
  >("/instances/:id/cookie", safeRoute(async (ctx: RouterContext) => {
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
    if (!ok) {
      return { status: 500, body: { error: "failed to set cookie" } };
    }
    return { status: 200, body: { ok: true } };
  }));

  // Accept alert
  ns.post<unknown, { ok?: boolean; error?: string }>(
    "/instances/:id/acceptAlert",
    safeRoute(async (ctx: RouterContext) => {
      const service = getService();
      if (!service.acceptAlert) {
        return { status: 501, body: { error: "acceptAlert not supported" } };
      }
      const ok = await service.acceptAlert(ctx.params.id);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Dismiss alert
  ns.post<unknown, { ok?: boolean; error?: string }>(
    "/instances/:id/dismissAlert",
    safeRoute(async (ctx: RouterContext) => {
      const service = getService();
      if (!service.dismissAlert) {
        return { status: 501, body: { error: "dismissAlert not supported" } };
      }
      const ok = await service.dismissAlert(ctx.params.id);
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Wait for network idle (supports SPA ignore patterns)
  ns.post<{ timeout?: number }, { ok?: boolean; error?: string }>(
    "/instances/:id/waitForNetworkIdle",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        timeout?: number;
        maxInflight?: number;
        idleDuration?: number;
        ignorePatterns?: string[];
      } | null;
      const service = getService();
      if (!service.waitForNetworkIdle) {
        return {
          status: 501,
          body: { error: "waitForNetworkIdle not supported" },
        };
      }
      const ok = await service.waitForNetworkIdle(ctx.params.id, {
        timeout: clampTimeout(json?.timeout, 5000),
        maxInflight: typeof json?.maxInflight === "number" ? json.maxInflight : undefined,
        idleDuration: typeof json?.idleDuration === "number" ? json.idleDuration : undefined,
        ignorePatterns: Array.isArray(json?.ignorePatterns) ? json.ignorePatterns : undefined,
      });
      return { status: 200, body: { ok: ok ?? false } };
    }),
  );

  // Wait for document ready (DOMContentLoaded)
  ns.post("/instances/:id/waitForReady", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as { timeout?: number } | null;
    const timeout = clampTimeout(json?.timeout, 15000);
    const service = getService();
    const ok = await service.waitForReady(ctx.params.id, timeout);
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Set innerHTML (for contenteditable elements, selector or fingerprint)
  ns.post<
    { fingerprint: string; html?: string },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/setInnerHTML", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint?: string;
      html?: string;
    } | null;
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const html = json?.html ?? "";
    const ok = await service.setInnerHTML(
      ctx.params.id,
      resolved.selector,
      html,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Set textContent (for contenteditable elements, selector or fingerprint)
  ns.post<
    { fingerprint: string; text?: string },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/setTextContent", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint?: string;
      text?: string;
    } | null;
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const text = json?.text ?? "";
    const ok = await service.setTextContent(
      ctx.params.id,
      resolved.selector,
      text,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Dispatch event on element (selector or fingerprint)
  ns.post<
    {
      fingerprint: string;
      selector?: string;
      eventType?: string;
      options?: { bubbles?: boolean; cancelable?: boolean };
    },
    { ok: boolean } | ErrorResponse
  >("/instances/:id/dispatchEvent", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint: string;
      eventType?: string;
      options?: { bubbles?: boolean; cancelable?: boolean };
    } | null;
    if (!json?.eventType) {
      return { status: 400, body: { error: "eventType required" } };
    }
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;
    const options = json?.options;
    const ok = await service.dispatchEvent(
      ctx.params.id,
      resolved.selector,
      json.eventType,
      options,
    );
    return { status: 200, body: { ok: ok ?? false } };
  }));

  // Type into an element (optional typing mode, selector or fingerprint)
  ns.post<
    {
      fingerprint: string;
      selector?: string;
      value?: string;
      typingMode?: boolean;
      typingDelayMs?: number;
    },
    OkResponse | ErrorResponse
  >("/instances/:id/input", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint: string;
      value?: string;
      typingMode?: boolean;
      typingDelayMs?: number;
    } | null;

    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;

    if (json?.value === undefined) {
      return { status: 400, body: { error: "value required" } };
    }
    if (!service.inputElement) {
      return { status: 501, body: { error: "inputElement not supported" } };
    }
    const ok = await service.inputElement(
      ctx.params.id,
      resolved.selector,
      json.value,
      {
        typingMode: json.typingMode,
        typingDelayMs: json.typingDelayMs,
      },
    );
    return { status: 200, body: { ok: !!ok } };
  }));

  // Press key or key combination
  ns.post<{ key?: string }, OkResponse | ErrorResponse>(
    "/instances/:id/pressKey",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as { key?: string } | null;
      if (!json?.key) {
        return { status: 400, body: { error: "key required" } };
      }
      const service = getService();
      if (!service.pressKey) {
        return { status: 501, body: { error: "pressKey not supported" } };
      }
      const ok = await service.pressKey(ctx.params.id, json.key);
      return { status: 200, body: { ok: !!ok } };
    }),
  );

  // Upload file via input[type=file] (selector or fingerprint)
  ns.post<
    { fingerprint: string; filePath?: string },
    OkResponse | ErrorResponse
  >("/instances/:id/uploadFile", safeRoute(async (ctx: RouterContext) => {
    const json = ctx.json() as {
      selector?: string;
      fingerprint?: string;
      filePath?: string;
    } | null;
    const service = getService();
    const resolved = await resolveSelectorOrFingerprint(
      service,
      ctx.params.id,
      json?.selector,
      json?.fingerprint,
    );
    if (!resolved.ok) return resolved.error;

    if (!json?.filePath) {
      return { status: 400, body: { error: "filePath required" } };
    }
    if (!service.uploadFile) {
      return { status: 501, body: { error: "uploadFile not supported" } };
    }
    const ok = await service.uploadFile(
      ctx.params.id,
      resolved.selector,
      json.filePath,
    );
    return { status: 200, body: { ok: !!ok } };
  }));

  // Dispatch text input event (for rich text editors, selector or fingerprint)
  ns.post<{ fingerprint: string; text?: string }, OkResponse | ErrorResponse>(
    "/instances/:id/dispatchTextInput",
    safeRoute(async (ctx: RouterContext) => {
      const json = ctx.json() as {
        selector?: string;
        fingerprint?: string;
        text?: string;
      } | null;
      const service = getService();
      const resolved = await resolveSelectorOrFingerprint(
        service,
        ctx.params.id,
        json?.selector,
        json?.fingerprint,
      );
      if (!resolved.ok) return resolved.error;

      if (json?.text === undefined) {
        return { status: 400, body: { error: "text required" } };
      }
      const ok = await service.dispatchTextInput(
        ctx.params.id,
        resolved.selector,
        json.text,
      );
      return { status: 200, body: { ok: !!ok } };
    }),
  );
}
