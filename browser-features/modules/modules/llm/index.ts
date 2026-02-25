/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LLM Agent Module - Public API
 *
 * This module provides LLM-powered browser automation capabilities
 * by integrating with the WebScraper infrastructure.
 */

// Main service
export {
  LLMAgentService,
  getLLMAgentService,
  initLLMAgentService,
} from "./LLMAgentService.sys.mts";

// Providers
export { OpenAIProvider } from "./providers/OpenAIProvider.ts";
export { AnthropicProvider } from "./providers/AnthropicProvider.ts";
export { OllamaProvider } from "./providers/OllamaProvider.ts";
export { BaseLLMProvider } from "./providers/Provider.ts";
export type { LLMProvider } from "./providers/Provider.ts";

// Provider Registry
export {
  ProviderRegistry,
  getProviderRegistry,
  resetProviderRegistry,
  PROVIDER_METADATA,
} from "./providers/ProviderRegistry.ts";
export type { ProviderMetadata } from "./providers/ProviderRegistry.ts";

// Tools
export {
  ToolRegistry,
  WebScraperTool,
  BaseTool,
  defineTool,
} from "./tools/ToolRegistry.ts";
export type { Tool, ToolContext } from "./tools/Tool.ts";

// Context
export {
  AgentContext,
  createBrowserAutomationContext,
} from "./context/AgentContext.ts";

// Types
export type {
  Message,
  ContentBlock,
  ToolDefinition,
  ToolCall,
  ToolResult,
  CompletionOptions,
  CompletionResponse,
  StreamingCallback,
  LLMProviderConfig,
  AgentSession,
} from "./types.ts";

export { PREF_KEYS } from "./types.ts";
