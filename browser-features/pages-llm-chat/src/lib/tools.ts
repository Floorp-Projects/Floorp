import type { LLMProviderSettings } from "../types.ts";
import { createBirpc } from "birpc";
import type { TabInfo } from "#modules/common/defines.ts";

// Tool definitions for OpenAI-compatible function calling
export const BROWSER_TOOLS = [
  {
    type: "function" as const,
    function: {
      name: "list_tabs",
      description:
        "List all open browser tabs. Returns tab IDs, titles, and URLs.",
      parameters: {
        type: "object",
        properties: {},
        required: [],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "create_tab",
      description:
        "Create a new browser tab and navigate to a URL. Returns a tab instance ID for further operations.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to open in the new tab",
          },
        },
        required: ["url"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "navigate_tab",
      description:
        "Navigate an existing tab to a new URL. Requires a tab instance ID.",
      parameters: {
        type: "object",
        properties: {
          tab_id: {
            type: "string",
            description: "The tab instance ID to navigate",
          },
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
        },
        required: ["tab_id", "url"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "close_tab",
      description: "Close a browser tab by its instance ID.",
      parameters: {
        type: "object",
        properties: {
          tab_id: {
            type: "string",
            description: "The tab instance ID to close",
          },
        },
        required: ["tab_id"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "get_browser_history",
      description:
        "Get recent browser history. Returns visited URLs and titles.",
      parameters: {
        type: "object",
        properties: {
          limit: {
            type: "number",
            description:
              "Maximum number of history items to return (default: 10)",
          },
        },
        required: [],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "search_web",
      description:
        "Open a search in a new tab using the user's default search engine or a specific search URL.",
      parameters: {
        type: "object",
        properties: {
          query: {
            type: "string",
            description: "The search query",
          },
        },
        required: ["query"],
      },
    },
  },
  // Scraper tools - Content reading
  {
    type: "function" as const,
    function: {
      name: "read_page_content",
      description:
        "Read the HTML or text content of a web page. Creates a scraper instance, navigates to URL, and returns content.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to read content from",
          },
          format: {
            type: "string",
            enum: ["html", "text"],
            description:
              "Return format: 'html' for raw HTML, 'text' for plain text (default: text)",
          },
        },
        required: ["url"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "read_element",
      description:
        "Read text content from a specific element on a page using CSS selector.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description:
              "CSS selector for the element to read (e.g., 'h1', '.article-content', '#main')",
          },
        },
        required: ["url", "selector"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "read_elements",
      description:
        "Read text content from multiple elements matching a CSS selector.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description:
              "CSS selector for elements to read (e.g., 'a', 'p', '.item')",
          },
        },
        required: ["url", "selector"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "get_element_attribute",
      description:
        "Get an attribute value from an element (e.g., 'href' from links, 'src' from images).",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description: "CSS selector for the element",
          },
          attribute: {
            type: "string",
            description:
              "The attribute name to get (e.g., 'href', 'src', 'data-id')",
          },
        },
        required: ["url", "selector", "attribute"],
      },
    },
  },
  // Scraper tools - Actions
  {
    type: "function" as const,
    function: {
      name: "click_element",
      description:
        "Click an element on a web page. Useful for buttons, links, and interactive elements.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description: "CSS selector for the element to click",
          },
          wait_for: {
            type: "string",
            description: "Optional CSS selector to wait for after clicking",
          },
        },
        required: ["url", "selector"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "fill_form",
      description:
        "Fill form fields on a web page. Provide field selectors and values.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          fields: {
            type: "array",
            items: {
              type: "object",
              properties: {
                selector: {
                  type: "string",
                  description: "CSS selector for the input field",
                },
                value: { type: "string", description: "Value to fill in" },
              },
              required: ["selector", "value"],
            },
            description: "Array of field selector and value pairs",
          },
        },
        required: ["url", "fields"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "submit_form",
      description:
        "Submit a form on a web page by clicking the submit button or form element.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description: "CSS selector for the submit button or form to submit",
          },
        },
        required: ["url", "selector"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "wait_for_element",
      description:
        "Wait for an element to appear on the page. Useful for dynamic content.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description: "CSS selector for the element to wait for",
          },
          timeout: {
            type: "number",
            description: "Maximum wait time in milliseconds (default: 5000)",
          },
        },
        required: ["url", "selector"],
      },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "take_screenshot",
      description: "Take a screenshot of a web page or specific element.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to navigate to",
          },
          selector: {
            type: "string",
            description:
              "Optional CSS selector to screenshot only a specific element",
          },
        },
        required: ["url"],
      },
    },
  },
];

// Tool call type
export interface ToolCall {
  id: string;
  type: "function";
  function: {
    name: string;
    arguments: string;
  };
}

// Tool result type
export interface ToolResult {
  tool_call_id: string;
  content: string;
}

// RPC functions available from the parent (Firefox chrome context) for Floorp tools
interface FloorpParentFunctions {
  listTabs: () => Promise<TabInfo[]>;
  createTab: (
    url: string,
  ) => Promise<{ success: boolean; id?: string; error?: string }>;
  navigateTab: (
    tabId: string,
    url: string,
  ) => Promise<{ success: boolean; error?: string }>;
  closeTab: (tabId: string) => Promise<{ success: boolean; error?: string }>;
  searchWeb: (query: string) => Promise<{ success: boolean; error?: string }>;
  getBrowserHistory: (
    limit: number,
  ) => Promise<Array<{ url: string; title: string }>>;
  createScraperInstance: () => Promise<{
    success: boolean;
    id?: string;
    error?: string;
  }>;
  destroyScraperInstance: (
    id: string,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperNavigate: (
    id: string,
    url: string,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperGetHtml: (id: string) => Promise<{ html: string }>;
  scraperGetText: (id: string) => Promise<{ text: string }>;
  scraperGetElementText: (
    id: string,
    selector: string,
  ) => Promise<{ text: string }>;
  scraperGetElements: (
    id: string,
    selector: string,
  ) => Promise<{ elements: Array<{ text: string; html: string }> }>;
  scraperGetElementAttribute: (
    id: string,
    selector: string,
    attribute: string,
  ) => Promise<{ value: string }>;
  scraperClick: (
    id: string,
    selector: string,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperFillForm: (
    id: string,
    fields: Array<{ selector: string; value: string }>,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperClearInput: (
    id: string,
    selector: string,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperSubmit: (
    id: string,
    selector: string,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperWaitForSelector: (
    id: string,
    selector: string,
    timeout: number,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperWaitForElement: (
    id: string,
    selector: string,
    timeout?: number,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperWaitForNavigation: (
    id: string,
    timeout: number,
  ) => Promise<{ success: boolean; error?: string }>;
  scraperExecuteScript: (
    id: string,
    script: string,
  ) => Promise<{ value: string }>;
  scraperScreenshot: (id: string) => Promise<{ data: string; error?: string }>;
  scraperTakeScreenshot: (
    id: string,
    selector?: string,
  ) => Promise<{ data: string }>;
  scraperExtractLinks: (
    id: string,
  ) => Promise<{ links: Array<{ text: string; href: string }> }>;
}

// Floorp Actor client using birpc
class FloorpActorClient {
  private rpc: FloorpParentFunctions | null = null;
  private initPromise: Promise<void> | null = null;

  constructor() {
    this.initPromise = this.initialize();
  }

  private async initialize(): Promise<void> {
    // Check if NRSettings functions are available
    if (typeof globalThis === "undefined") {
      throw new Error("Window not available");
    }

    const win = globalThis as Window &
      typeof globalThis & {
        NRSettingsRegisterReceiveCallback?: (
          callback: (data: string) => void,
        ) => void;
        NRSettingsSend?: (data: string) => void;
        NRSPing?: () => boolean;
      };

    // Wait for Actor to be ready
    let attempts = 0;
    while (!win.NRSettingsRegisterReceiveCallback && attempts < 50) {
      await new Promise((resolve) => globalThis.setTimeout(resolve, 100));
      attempts++;
    }

    if (!win.NRSettingsRegisterReceiveCallback) {
      throw new Error("NRSettings Actor not available");
    }

    // Create birpc connection
    // birpc generics: <RemoteFunctions, LocalFunctions>
    // RemoteFunctions: what we call (FloorpParentFunctions)
    // LocalFunctions: what we expose (nothing)
    // The returned object is a proxy that implements RemoteFunctions
    const rpcClient = createBirpc<FloorpParentFunctions, Record<string, never>>(
      {}, // No local functions to expose
      {
        post: (data) => {
          if (win.NRSettingsSend) {
            win.NRSettingsSend(data);
          }
        },
        on: (callback) => {
          win.NRSettingsRegisterReceiveCallback!((data: string) => {
            callback(data);
          });
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
    // birpc returns the proxy for the remote functions
    this.rpc = rpcClient;
  }

  private async ensureReady(): Promise<void> {
    if (this.initPromise) {
      await this.initPromise;
    }
    if (!this.rpc) {
      throw new Error("Actor client not initialized");
    }
  }

  async listTabs(): Promise<TabInfo[]> {
    await this.ensureReady();
    return this.rpc!.listTabs();
  }

  async createTab(
    url: string,
  ): Promise<{ success: boolean; id?: string; error?: string }> {
    await this.ensureReady();
    return this.rpc!.createTab(url);
  }

  async navigateTab(
    tabId: string,
    url: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.navigateTab(tabId, url);
  }

  async closeTab(tabId: string): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.closeTab(tabId);
  }

  async searchWeb(
    query: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.searchWeb(query);
  }

  async getBrowserHistory(
    limit: number = 10,
  ): Promise<Array<{ url: string; title: string }>> {
    await this.ensureReady();
    return this.rpc!.getBrowserHistory(limit);
  }

  // Scraper operations
  async createScraperInstance(): Promise<{
    success: boolean;
    id?: string;
    error?: string;
  }> {
    await this.ensureReady();
    return this.rpc!.createScraperInstance();
  }

  async destroyScraperInstance(
    id: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.destroyScraperInstance(id);
  }

  async scraperNavigate(
    id: string,
    url: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperNavigate(id, url);
  }

  async scraperGetHtml(id: string): Promise<{ html: string }> {
    await this.ensureReady();
    return this.rpc!.scraperGetHtml(id);
  }

  async scraperGetText(id: string): Promise<{ text: string }> {
    await this.ensureReady();
    return this.rpc!.scraperGetText(id);
  }

  async scraperGetElementText(
    id: string,
    selector: string,
  ): Promise<{ text: string }> {
    await this.ensureReady();
    return this.rpc!.scraperGetElementText(id, selector);
  }

  async scraperGetElements(
    id: string,
    selector: string,
  ): Promise<{ elements: Array<{ text: string; html: string }> }> {
    await this.ensureReady();
    return this.rpc!.scraperGetElements(id, selector);
  }

  async scraperGetElementAttribute(
    id: string,
    selector: string,
    attribute: string,
  ): Promise<{ value: string }> {
    await this.ensureReady();
    return this.rpc!.scraperGetElementAttribute(id, selector, attribute);
  }

  async scraperClick(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperClick(id, selector);
  }

  async scraperFillForm(
    id: string,
    fields: Array<{ selector: string; value: string }>,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperFillForm(id, fields);
  }

  async scraperClearInput(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperClearInput(id, selector);
  }

  async scraperSubmit(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperSubmit(id, selector);
  }

  async scraperWaitForElement(
    id: string,
    selector: string,
    timeout?: number,
  ): Promise<{ success: boolean; error?: string }> {
    await this.ensureReady();
    return this.rpc!.scraperWaitForElement(id, selector, timeout);
  }

  async scraperExecuteScript(
    id: string,
    script: string,
  ): Promise<{ value: string }> {
    await this.ensureReady();
    return this.rpc!.scraperExecuteScript(id, script);
  }

  async scraperTakeScreenshot(
    id: string,
    selector?: string,
  ): Promise<{ data: string }> {
    await this.ensureReady();
    return this.rpc!.scraperTakeScreenshot(id, selector);
  }
}

// Global client instance
let floorpClient: FloorpActorClient | null = null;

export function getFloorpClient(): FloorpActorClient {
  if (!floorpClient) {
    floorpClient = new FloorpActorClient();
  }
  return floorpClient;
}

function formatToolError(error: unknown, depth = 0): string {
  if (error instanceof Error) {
    return error.message || error.name;
  }

  if (typeof error === "string") {
    const trimmed = error.trim();
    return trimmed === "" ? "Unknown error" : trimmed;
  }

  if (
    typeof error === "number" ||
    typeof error === "boolean" ||
    typeof error === "bigint"
  ) {
    return String(error);
  }

  if (error == null) {
    return "Unknown error";
  }

  if (typeof error !== "object") {
    return String(error);
  }

  if (depth > 2) {
    return "Unknown error object";
  }

  const value = error as Record<string, unknown>;
  const message = typeof value.message === "string" ? value.message : null;
  const errorField = value.error;
  const cause = value.cause;
  const code = typeof value.code === "string" ? value.code : null;
  const name = typeof value.name === "string" ? value.name : null;

  if (message && code) {
    return `${code}: ${message}`;
  }
  if (message && name && name !== "Error") {
    return `${name}: ${message}`;
  }
  if (message) {
    return message;
  }

  if (typeof errorField === "string" && errorField.trim() !== "") {
    return errorField;
  }
  if (errorField !== undefined) {
    return formatToolError(errorField, depth + 1);
  }

  if (typeof cause === "string" && cause.trim() !== "") {
    return cause;
  }
  if (cause !== undefined) {
    return formatToolError(cause, depth + 1);
  }

  try {
    const seen = new WeakSet<object>();
    const json = JSON.stringify(error, (_key, currentValue) => {
      if (currentValue && typeof currentValue === "object") {
        if (seen.has(currentValue)) {
          return "[Circular]";
        }
        seen.add(currentValue);
      }
      return currentValue;
    });
    if (!json || json === "{}" || json === "[]") {
      return "Unknown RPC error";
    }
    return json;
  } catch {
    return "Unknown RPC error";
  }
}

function getResultError(result: unknown): string | null {
  if (!result || typeof result !== "object") {
    return null;
  }

  if (!("error" in result)) {
    return null;
  }

  const errorValue = (result as { error?: unknown }).error;
  if (errorValue == null || errorValue === "") {
    return null;
  }

  return formatToolError(errorValue);
}

function isLikelyTransientScraperError(error: unknown): boolean {
  const message = formatToolError(error).toLowerCase();
  return (
    message === "unknown rpc error" ||
    message.includes("no active tab actor") ||
    message.includes("dead object") ||
    message.includes("ns_error") ||
    message.includes("channel closing") ||
    message.includes("actor")
  );
}

async function sleep(ms: number): Promise<void> {
  await new Promise((resolve) => globalThis.setTimeout(resolve, ms));
}

async function waitForScraperActorReady(
  client: FloorpActorClient,
  scraperId: string,
): Promise<void> {
  let lastError: unknown = null;

  for (let attempt = 0; attempt < 12; attempt++) {
    try {
      const probe = await client.scraperGetHtml(scraperId);
      const probeError = getResultError(probe);
      if (!probeError) {
        return;
      }
      lastError = probeError;
      if (!isLikelyTransientScraperError(probeError)) {
        return;
      }
    } catch (error) {
      lastError = error;
      if (!isLikelyTransientScraperError(error)) {
        return;
      }
    }

    await sleep(250);
  }

  // Best-effort wait only: don't fail the tool here, leave detailed errors to the actual operation.
  if (lastError) {
    console.debug?.(
      "[pages-llm-chat] scraper actor readiness timeout",
      formatToolError(lastError),
    );
  }
}

async function waitForSelectorBeforeOperation(
  client: FloorpActorClient,
  scraperId: string,
  selector: string,
  timeout = 8000,
): Promise<string | null> {
  if (typeof selector !== "string" || selector.trim() === "") {
    return "selector must be a non-empty string";
  }

  for (let attempt = 0; attempt < 4; attempt++) {
    try {
      const result = await client.scraperWaitForElement(
        scraperId,
        selector,
        timeout,
      );
      if (result.success) {
        return null;
      }
      if (!isLikelyTransientScraperError(result.error)) {
        return formatToolError(result.error);
      }
    } catch (error) {
      if (!isLikelyTransientScraperError(error)) {
        return formatToolError(error);
      }
    }

    await sleep(300);
  }

  return `Element "${selector}" did not appear within ${timeout}ms`;
}

async function retryTransientScraperAction(
  client: FloorpActorClient,
  scraperId: string,
  run: () => Promise<{ success: boolean; error?: string }>,
): Promise<{ success: boolean; error?: string }> {
  const firstResult = await run();
  if (
    firstResult.success ||
    !isLikelyTransientScraperError(firstResult.error)
  ) {
    return firstResult;
  }

  await sleep(200);
  await waitForScraperActorReady(client, scraperId);
  return await run();
}

async function withNavigatedScraperSession(
  client: FloorpActorClient,
  url: string,
  run: (scraperId: string) => Promise<string>,
): Promise<string> {
  if (typeof url !== "string" || url.trim() === "") {
    return "Failed to navigate: url must be a non-empty string";
  }

  const createResult = await client.createScraperInstance();
  if (!createResult.success || !createResult.id) {
    return `Failed to create scraper instance: ${formatToolError(
      createResult.error,
    )}`;
  }

  try {
    const navResult = await client.scraperNavigate(createResult.id, url);
    if (!navResult.success) {
      return `Failed to navigate to ${url}: ${formatToolError(navResult.error)}`;
    }

    await waitForScraperActorReady(client, createResult.id);
    return await run(createResult.id);
  } finally {
    try {
      await client.destroyScraperInstance(createResult.id);
    } catch (error) {
      // Cleanup failure should not mask the main tool result.
      console.debug?.(
        "[pages-llm-chat] failed to destroy scraper instance",
        formatToolError(error),
      );
    }
  }
}

// Execute a tool call
export async function executeToolCall(toolCall: ToolCall): Promise<string> {
  const client = getFloorpClient();
  const { name, arguments: argsStr } = toolCall.function;

  try {
    const args = JSON.parse(argsStr) as Record<string, unknown>;

    switch (name) {
      case "list_tabs": {
        const tabs = await client.listTabs();
        return JSON.stringify(tabs, null, 2);
      }

      case "create_tab": {
        const result = await client.createTab(args.url as string);
        if (result.success) {
          return `Tab created successfully. ID: ${result.id}`;
        }
        return `Failed to create tab: ${result.error}`;
      }

      case "navigate_tab": {
        const result = await client.navigateTab(
          args.tab_id as string,
          args.url as string,
        );
        if (result.success) {
          return `Navigated tab ${args.tab_id as string} to ${args.url as string}`;
        }
        return `Failed to navigate tab: ${result.error}`;
      }

      case "close_tab": {
        const result = await client.closeTab(args.tab_id as string);
        if (result.success) {
          return `Tab ${args.tab_id as string} closed successfully`;
        }
        return `Failed to close tab: ${result.error}`;
      }

      case "get_browser_history": {
        const history = await client.getBrowserHistory(
          (args.limit as number) || 10,
        );
        return JSON.stringify(history, null, 2);
      }

      case "search_web": {
        const result = await client.searchWeb(args.query as string);
        if (result.success) {
          return `Opened search for "${args.query as string}" in new tab`;
        }
        return `Failed to search: ${result.error}`;
      }

      // Scraper tools - Content reading
      case "read_page_content": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const format = args.format === "html" ? "html" : "text";
            const content =
              format === "html"
                ? await client.scraperGetHtml(id)
                : await client.scraperGetText(id);
            const error = getResultError(content);
            if (error) {
              return `Failed to read page content: ${error}`;
            }
            return format === "html"
              ? (content as { html: string }).html
              : (content as { text: string }).text;
          },
        );
      }

      case "read_element": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const content = await client.scraperGetElementText(
              id,
              args.selector as string,
            );
            const error = getResultError(content);
            if (error) {
              return `Failed to read element "${args.selector as string}": ${error}`;
            }
            return content.text;
          },
        );
      }

      case "read_elements": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const waitError = await waitForSelectorBeforeOperation(
              client,
              id,
              args.selector as string,
              8000,
            );
            if (waitError) {
              return `Failed to read elements "${args.selector as string}": ${waitError}`;
            }
            const content = await client.scraperGetElements(
              id,
              args.selector as string,
            );
            const error = getResultError(content);
            if (error) {
              return `Failed to read elements "${args.selector as string}": ${error}`;
            }
            return JSON.stringify(content.elements, null, 2);
          },
        );
      }

      case "get_element_attribute": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const content = await client.scraperGetElementAttribute(
              id,
              args.selector as string,
              args.attribute as string,
            );
            const error = getResultError(content);
            if (error) {
              return `Failed to get attribute "${args.attribute as string}" from "${args.selector as string}": ${error}`;
            }
            return content.value;
          },
        );
      }

      // Scraper tools - Actions
      case "click_element": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const waitError = await waitForSelectorBeforeOperation(
              client,
              id,
              args.selector as string,
              10000,
            );
            if (waitError) {
              return `Failed to click element: ${waitError}`;
            }

            const clickResult = await retryTransientScraperAction(
              client,
              id,
              () => client.scraperClick(id, args.selector as string),
            );
            if (!clickResult.success) {
              return `Failed to click element: ${formatToolError(clickResult.error)}`;
            }
            if (args.wait_for) {
              const waitResult = await client.scraperWaitForElement(
                id,
                args.wait_for as string,
                5000,
              );
              if (!waitResult.success) {
                return `Clicked but failed to wait for element: ${waitResult.error}`;
              }
              return `Clicked element "${args.selector as string}" and waited for "${args.wait_for as string}"`;
            }
            return `Clicked element "${args.selector as string}"`;
          },
        );
      }

      case "fill_form": {
        if (!Array.isArray(args.fields)) {
          return "Failed to fill form: fields must be an array";
        }
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const firstField = (args.fields as Array<unknown>).find(
              (field: unknown) =>
                field &&
                typeof field === "object" &&
                typeof (field as { selector?: unknown }).selector === "string",
            ) as { selector: string } | undefined;
            if (firstField) {
              const waitError = await waitForSelectorBeforeOperation(
                client,
                id,
                firstField.selector,
                10000,
              );
              if (waitError) {
                return `Failed to fill form: ${waitError}`;
              }
            }
            const fillResult = await retryTransientScraperAction(
              client,
              id,
              () =>
                client.scraperFillForm(
                  id,
                  args.fields as Array<{ selector: string; value: string }>,
                ),
            );
            if (!fillResult.success) {
              return `Failed to fill form: ${formatToolError(fillResult.error)}`;
            }
            const filledCount = (args.fields as Array<unknown>).length;
            return `Filled ${filledCount} form field(s)`;
          },
        );
      }

      case "submit_form": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const submitResult = await client.scraperSubmit(
              id,
              args.selector as string,
            );
            if (!submitResult.success) {
              return `Failed to submit form: ${submitResult.error}`;
            }
            return `Submitted form via "${args.selector as string}"`;
          },
        );
      }

      case "wait_for_element": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const timeout = (args.timeout as number) || 5000;
            const waitResult = await client.scraperWaitForElement(
              id,
              args.selector as string,
              timeout,
            );
            if (!waitResult.success) {
              return `Element "${args.selector as string}" did not appear within ${timeout}ms`;
            }
            return `Element "${args.selector as string}" appeared`;
          },
        );
      }

      case "take_screenshot": {
        return await withNavigatedScraperSession(
          client,
          args.url as string,
          async (id) => {
            const screenshot = await client.scraperTakeScreenshot(
              id,
              args.selector as string | undefined,
            );
            const screenshotError = getResultError(screenshot);
            if (screenshotError) {
              return `Failed to take screenshot: ${screenshotError}`;
            }
            if (!screenshot.data) {
              return "Failed to take screenshot: empty image data";
            }
            const description = args.selector
              ? `Screenshot of element "${args.selector as string}" on ${args.url as string}`
              : `Full page screenshot of ${args.url as string}`;
            return `${description}\n\nBase64 image data (${Math.round(screenshot.data.length / 1024)}KB):\n${screenshot.data.substring(0, 100)}...`;
          },
        );
      }

      default:
        return `Unknown tool: ${name}`;
    }
  } catch (error) {
    const message = formatToolError(error);
    return `Error executing ${name}: ${message}`;
  }
}

// Check if tools are supported by the provider
export function supportsToolCalling(settings: LLMProviderSettings): boolean {
  return (
    settings.type === "openai" ||
    settings.type === "ollama" ||
    settings.type === "openai-compatible"
  );
}
