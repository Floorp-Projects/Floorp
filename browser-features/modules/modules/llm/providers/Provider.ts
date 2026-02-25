/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Abstract interface for LLM providers
 */

import type {
  Message,
  CompletionOptions,
  CompletionResponse,
  StreamingCallback,
  ToolDefinition,
} from "../types.ts";

export interface LLMProvider {
  /**
   * Provider type identifier
   */
  readonly type: string;

  /**
   * Default model for this provider
   */
  readonly defaultModel: string;

  /**
   * Send a completion request (non-streaming)
   */
  complete(
    messages: Message[],
    options?: CompletionOptions,
  ): Promise<CompletionResponse>;

  /**
   * Send a streaming completion request
   */
  completeStreaming(
    messages: Message[],
    onChunk: StreamingCallback,
    options?: CompletionOptions,
  ): Promise<void>;

  /**
   * Check if the provider is properly configured
   */
  isConfigured(): boolean;

  /**
   * Get available models (optional, for providers that support it)
   */
  getAvailableModels?(): Promise<string[]>;

  /**
   * Count tokens in a message (optional, for context management)
   */
  countTokens?(messages: Message[]): number;
}

/**
 * Base class for LLM providers with common utilities
 */
export abstract class BaseLLMProvider implements LLMProvider {
  abstract readonly type: string;
  abstract readonly defaultModel: string;

  protected apiKey?: string;
  protected baseUrl?: string;
  protected model!: string; // Set by subclass
  protected timeout: number;

  constructor(config: {
    apiKey?: string;
    baseUrl?: string;
    model?: string;
    timeout?: number;
  }) {
    this.apiKey = config.apiKey;
    this.baseUrl = config.baseUrl;
    this.timeout = config.timeout ?? 60000;
    // model is set by subclass after super() call
    if (config.model) {
      this.model = config.model;
    }
  }

  abstract complete(
    messages: Message[],
    options?: CompletionOptions,
  ): Promise<CompletionResponse>;

  abstract completeStreaming(
    messages: Message[],
    onChunk: StreamingCallback,
    options?: CompletionOptions,
  ): Promise<void>;

  abstract isConfigured(): boolean;

  /**
   * Format messages into provider-specific format
   */
  protected abstract formatMessages(
    messages: Message[],
    systemPrompt?: string,
  ): unknown;

  /**
   * Format tools into provider-specific format
   */
  protected formatTools(tools: ToolDefinition[]): unknown {
    return tools;
  }

  /**
   * Make HTTP request with timeout
   */
  protected async fetchWithTimeout(
    url: string,
    options: RequestInit,
    timeout: number,
  ): Promise<Response> {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    try {
      const response = await fetch(url, {
        ...options,
        signal: controller.signal,
      });
      return response;
    } finally {
      clearTimeout(timeoutId);
    }
  }

  /**
   * Build authorization header
   */
  protected getAuthHeader(
    headerName: string = "Authorization",
  ): Record<string, string> {
    if (!this.apiKey) return {};
    if (headerName === "Authorization") {
      return { Authorization: `Bearer ${this.apiKey}` };
    }
    return { [headerName]: this.apiKey };
  }
}
