// SPDX-License-Identifier: MPL-2.0

import { type Accessor, createSignal } from "solid-js";
import { analyzeSilverPassText } from "./silver-pass-analyzer.ts";
import type {
  SilverPassI18nKey,
  SilverPassPageClient,
  SilverPassPageContext,
  SilverPassPageContextProvider,
  SilverPassTargetKey,
  SilverPassViewState,
} from "./types.ts";

export const SILVER_PASS_DEMO_URL =
  "http://127.0.0.1:4173/municipal-application.html";
export const SILVER_PASS_DEMO_MARKER =
  'main[data-silver-pass-demo="municipal-application-v1"]';

const TARGET_SELECTORS: Readonly<Record<SilverPassTargetKey, string>> = {
  "apply-start": '#apply-start[data-silver-pass-target="apply-start"]',
};

const INITIAL_STATE: SilverPassViewState = {
  status: "idle",
  analysis: null,
  messageKey: null,
};

type WebScraperMessage =
  | "WebScraper:WaitForReady"
  | "WebScraper:IsVisible"
  | "WebScraper:GetText"
  | "WebScraper:ClearEffects"
  | "WebScraper:ScrollToElement";

interface WebScraperActor {
  sendQuery(
    messageName: WebScraperMessage,
    data?: Readonly<Record<string, unknown>>,
  ): Promise<unknown>;
}

function queryBoolean(
  actor: WebScraperActor,
  messageName: WebScraperMessage,
  data: Record<string, unknown> = {},
): Promise<boolean> {
  return actor.sendQuery(messageName, data).then((result: unknown) =>
    result === true
  );
}

class WindowActorSilverPassClient implements SilverPassPageClient {
  constructor(private readonly actor: WebScraperActor) {}

  waitForReady(): Promise<boolean> {
    return queryBoolean(this.actor, "WebScraper:WaitForReady", {
      timeout: 5000,
    });
  }

  isVisible(selector: string): Promise<boolean> {
    return queryBoolean(this.actor, "WebScraper:IsVisible", { selector });
  }

  async getScopedText(selector: string): Promise<string | null> {
    const result: unknown = await this.actor.sendQuery("WebScraper:GetText", {
      mode: "scoped",
      selector,
      enableFingerprints: false,
      includeSelectorMap: false,
      includeIframes: false,
    });
    if (typeof result === "string" || result === null) {
      return result;
    }
    throw new TypeError("NRWebScraper returned an invalid text response");
  }

  clearEffects(): Promise<boolean> {
    return queryBoolean(this.actor, "WebScraper:ClearEffects");
  }

  scrollToElement(selector: string): Promise<boolean> {
    return queryBoolean(this.actor, "WebScraper:ScrollToElement", {
      selector,
    });
  }
}

export function getSelectedSilverPassPageContext(): SilverPassPageContext {
  const browser = globalThis.gBrowser?.selectedBrowser as
    | XULBrowserElement
    | undefined;
  const url = browser?.currentURI?.spec ?? "";

  return {
    url,
    isCurrent(): boolean {
      return globalThis.gBrowser?.selectedBrowser === browser &&
        isSameSilverPassDocument(browser?.currentURI?.spec ?? "", url);
    },
    createClient(): SilverPassPageClient | null {
      try {
        const windowGlobal = browser?.browsingContext?.currentWindowGlobal;
        if (!windowGlobal || windowGlobal.isClosed) return null;
        const actor = windowGlobal.getActor(
          "NRWebScraper",
        ) as unknown as WebScraperActor;
        return actor ? new WindowActorSilverPassClient(actor) : null;
      } catch (error) {
        console.error("[SilverPass] Failed to acquire NRWebScraper:", error);
        return null;
      }
    },
  };
}

export function isSameSilverPassDocument(
  currentUrl: string,
  expectedUrl: string,
): boolean {
  return currentUrl.split("#", 1)[0] === expectedUrl.split("#", 1)[0];
}

export function isSilverPassDemoUrl(value: string): boolean {
  return value === SILVER_PASS_DEMO_URL ||
    value.startsWith(`${SILVER_PASS_DEMO_URL}#`);
}

export class SilverPassManager {
  private readonly stateSignal = createSignal<SilverPassViewState>(
    INITIAL_STATE,
  );
  readonly state: Accessor<SilverPassViewState> = this.stateSignal[0];
  private operationId = 0;

  constructor(
    private readonly getPageContext: SilverPassPageContextProvider =
      getSelectedSilverPassPageContext,
  ) {}

  destroy(): void {
    this.operationId += 1;
  }

  async analyzeCurrentPage(): Promise<void> {
    const operationId = ++this.operationId;
    this.setStateIfCurrent(operationId, {
      status: "loading",
      analysis: null,
      messageKey: null,
    });

    const page = this.getPageContext();
    if (!isSilverPassDemoUrl(page.url)) {
      this.setUnsupported(operationId);
      return;
    }
    if (!this.isCurrentPage(operationId, page)) {
      this.setUnsupported(operationId);
      return;
    }

    const client = page.createClient();
    if (!client) {
      this.setError(operationId, "silverPassDemo.errorActorUnavailable");
      return;
    }

    try {
      const ready = await client.waitForReady();
      if (!this.isCurrentPage(operationId, page)) return;
      if (!ready) {
        this.setError(operationId, "silverPassDemo.errorNotReady");
        return;
      }

      const markerVisible = await client.isVisible(SILVER_PASS_DEMO_MARKER);
      if (!this.isCurrentPage(operationId, page)) return;
      if (!markerVisible) {
        this.setUnsupported(operationId);
        return;
      }

      await client.clearEffects();
      if (!this.isCurrentPage(operationId, page)) return;

      const text = await client.getScopedText(SILVER_PASS_DEMO_MARKER);
      if (!this.isCurrentPage(operationId, page)) return;
      if (!text) {
        this.setError(
          operationId,
          "silverPassDemo.errorTextRetrievalFailed",
        );
        return;
      }

      const analysis = analyzeSilverPassText(text);
      if (!analysis) {
        this.setUnsupported(operationId);
        return;
      }

      const targetSelector = TARGET_SELECTORS[analysis.targetKey];
      const targetVisible = await client.isVisible(targetSelector);
      if (!this.isCurrentPage(operationId, page)) return;
      if (!targetVisible) {
        this.setError(operationId, "silverPassDemo.errorGeneric");
        return;
      }

      this.setStateIfCurrent(operationId, {
        status: "ready",
        analysis,
        messageKey: null,
      });
    } catch (error) {
      if (!this.isCurrentPage(operationId, page)) return;
      console.error("[SilverPass] Failed to analyze demo page:", error);
      this.setError(operationId, "silverPassDemo.errorGeneric");
    }
  }

  async highlightNextStep(): Promise<boolean> {
    const operationId = ++this.operationId;
    const analysis = this.state().analysis;
    if (!analysis) {
      this.setUnsupported(operationId);
      return false;
    }

    const page = this.getPageContext();
    if (!isSilverPassDemoUrl(page.url)) {
      this.setUnsupported(operationId);
      return false;
    }
    if (!this.isCurrentPage(operationId, page)) {
      this.setUnsupported(operationId);
      return false;
    }

    const client = page.createClient();
    if (!client) {
      this.setError(operationId, "silverPassDemo.errorActorUnavailable");
      return false;
    }

    try {
      const ready = await client.waitForReady();
      if (!this.isCurrentPage(operationId, page)) return false;
      if (!ready) {
        this.setError(operationId, "silverPassDemo.errorNotReady");
        return false;
      }

      const markerVisible = await client.isVisible(SILVER_PASS_DEMO_MARKER);
      if (!this.isCurrentPage(operationId, page)) return false;
      if (!markerVisible) {
        this.setUnsupported(operationId);
        return false;
      }

      const targetSelector = TARGET_SELECTORS[analysis.targetKey];
      const targetVisible = await client.isVisible(targetSelector);
      if (!this.isCurrentPage(operationId, page)) return false;
      if (!targetVisible) {
        this.setError(operationId, "silverPassDemo.errorGeneric");
        return false;
      }

      await client.clearEffects();
      if (!this.isCurrentPage(operationId, page)) return false;

      const highlighted = await client.scrollToElement(targetSelector);
      if (!highlighted) {
        this.setError(operationId, "silverPassDemo.errorGeneric");
        return false;
      }
      return this.isCurrentPage(operationId, page);
    } catch (error) {
      if (!this.isCurrentPage(operationId, page)) return false;
      console.error("[SilverPass] Failed to highlight next step:", error);
      this.setError(operationId, "silverPassDemo.errorGeneric");
      return false;
    }
  }

  private isCurrent(operationId: number): boolean {
    return operationId === this.operationId;
  }

  private isCurrentPage(
    operationId: number,
    page: SilverPassPageContext,
  ): boolean {
    return this.isCurrent(operationId) && (page.isCurrent?.() ?? true);
  }

  private setStateIfCurrent(
    operationId: number,
    state: SilverPassViewState,
  ): void {
    if (this.isCurrent(operationId)) {
      this.stateSignal[1](state);
    }
  }

  private setUnsupported(operationId: number): void {
    this.setStateIfCurrent(operationId, {
      status: "unsupported",
      analysis: null,
      messageKey: "silverPassDemo.unsupportedMessage",
    });
  }

  private setError(
    operationId: number,
    messageKey: SilverPassI18nKey,
  ): void {
    this.setStateIfCurrent(operationId, {
      status: "error",
      analysis: null,
      messageKey,
    });
  }
}
