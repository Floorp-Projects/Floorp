// SPDX-License-Identifier: MPL-2.0
import type { Experiment } from "#modules/modules/experiments/Experiments.sys.mts";

export interface PrefGetParams {
  prefName: string;
  prefType: "string" | "boolean" | "number";
}

export interface PrefSetParams {
  prefName: string;
  prefType: "string" | "boolean" | "number";
  prefValue: string | boolean | number;
}

export interface ActiveExperiment {
  id: string;
  variantId: string;
  variantName: string;
  assignedAt: string | null;
  experimentData: Experiment;
  disabled: boolean;
}

export interface AvailableExperiment {
  id: string;
  name: string | undefined;
  description: string | undefined;
  rollout: number;
  start: string | undefined;
  end: string | undefined;
  isActive: boolean;
  enrollmentStatus:
    | "enrolled"
    | "not_in_rollout"
    | "force_enrolled"
    | "disabled"
    | "control";
  currentVariantId: string | null;
  experimentData: Experiment;
}

export interface TabInfo {
  id: string;
  title: string;
  url: string;
}

// Shared LLM provider defaults for settings/chat/backend UIs.
// Note: Base URL semantics can differ by subsystem (e.g. Ollama /api vs /v1),
// so only model defaults and UI model lists are centralized here.
export const FLOORP_LLM_DEFAULT_MODELS = {
  openai: "gpt-5.2",
  anthropic: "claude-sonnet-4-6",
  ollama: "llama3.3",
  "openai-compatible": "",
  "anthropic-compatible": "",
  custom: "",
} as const;

export const FLOORP_LLM_MODEL_OPTIONS = {
  openai: [
    "gpt-5.2",
    "gpt-5.2-pro",
    "gpt-5.1",
    "gpt-5",
    "gpt-5-mini",
    "gpt-5-nano",
    "gpt-4.1",
    "gpt-4o",
    "gpt-4o-mini",
    "gpt-4-turbo",
    "o3",
    "o3-mini",
    "o4-mini",
  ],
  anthropic: [
    "claude-opus-4-6",
    "claude-sonnet-4-6",
    "claude-haiku-4-5",
    "claude-3-5-sonnet-20241022",
    "claude-3-5-haiku-20241022",
    "claude-3-opus-20240229",
    "claude-3-sonnet-20240229",
    "claude-3-haiku-20240307",
  ],
  ollama: [
    "llama4",
    "llama3.3",
    "llama3.2",
    "llama3.2-vision",
    "llama3.1",
    "deepseek-r1",
    "deepseek-v3",
    "deepseek-coder-v2",
    "qwen3",
    "qwen3-coder",
    "qwen2.5",
    "gemma3",
    "gemma2",
    "phi4",
    "phi4-mini",
    "phi3.5",
    "mistral",
    "mistral-nemo",
    "mixtral",
    "codellama",
    "command-r",
    "qwq",
  ],
} as const;

// Scraper instance types
export interface ScraperInstance {
  id: string;
  url: string;
}

export interface FormField {
  selector: string;
  value: string;
}

export interface WaitOptions {
  timeout?: number;
}

// Scraper response types
export interface ScraperCreateResponse {
  success: boolean;
  id?: string;
  error?: string;
}

export interface ScraperHtmlResponse {
  html: string;
  error?: string;
}

export interface ScraperTextResponse {
  text: string;
  error?: string;
}

export interface ScraperValueResponse {
  value: string;
  error?: string;
}

export interface ScraperElementsResponse {
  elements: string[];
  error?: string;
}

export interface ScraperScreenshotResponse {
  data: string; // base64
  error?: string;
}

export interface NRSettingsParentFunctions {
  getBoolPref(prefName: string): Promise<boolean | null>;
  getIntPref(prefName: string): Promise<number | null>;
  getStringPref(prefName: string): Promise<string | null>;
  setBoolPref(prefName: string, prefValue: boolean): Promise<void>;
  setIntPref(prefName: string, prefValue: number): Promise<void>;
  setStringPref(prefName: string, prefValue: string): Promise<void>;
  // Browser operations
  listTabs(): Promise<TabInfo[]>;
  createTab(
    url: string,
  ): Promise<{ success: boolean; id?: string; error?: string }>;
  navigateTab(
    tabId: string,
    url: string,
  ): Promise<{ success: boolean; error?: string }>;
  closeTab(tabId: string): Promise<{ success: boolean; error?: string }>;
  searchWeb(query: string): Promise<{ success: boolean; error?: string }>;
  getBrowserHistory(
    limit?: number,
  ): Promise<Array<{ url: string; title: string }>>;
  // Scraper operations - Content reading
  createScraperInstance(): Promise<ScraperCreateResponse>;
  destroyScraperInstance(
    id: string,
  ): Promise<{ success: boolean; error?: string }>;
  scraperNavigate(
    id: string,
    url: string,
  ): Promise<{ success: boolean; error?: string }>;
  scraperGetHtml(id: string): Promise<ScraperHtmlResponse>;
  scraperGetText(id: string): Promise<ScraperTextResponse>;
  scraperGetElementText(
    id: string,
    selector: string,
  ): Promise<ScraperTextResponse>;
  scraperGetElements(
    id: string,
    selector: string,
  ): Promise<ScraperElementsResponse>;
  scraperGetElementAttribute(
    id: string,
    selector: string,
    attribute: string,
  ): Promise<ScraperValueResponse>;
  // Scraper operations - Actions
  scraperClick(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }>;
  scraperFillForm(
    id: string,
    fields: FormField[],
  ): Promise<{ success: boolean; error?: string }>;
  scraperClearInput(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }>;
  scraperSubmit(
    id: string,
    selector: string,
  ): Promise<{ success: boolean; error?: string }>;
  scraperWaitForElement(
    id: string,
    selector: string,
    timeout?: number,
  ): Promise<{ success: boolean; error?: string }>;
  scraperExecuteScript(
    id: string,
    script: string,
  ): Promise<ScraperValueResponse>;
  // Scraper operations - Screenshot
  scraperTakeScreenshot(
    id: string,
    selector?: string,
  ): Promise<ScraperScreenshotResponse>;
}

export interface NRExperimemmtParentFunctions {
  getActiveExperiments(): Promise<ActiveExperiment[]>;
  getAllExperiments(): Promise<AvailableExperiment[]>;
  disableExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  enableExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  forceEnrollExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  removeForceEnrollment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  clearExperimentCache(): Promise<{ success: boolean; error?: string }>;
  reinitializeExperiments(): Promise<{ success: boolean; error?: string }>;
}
