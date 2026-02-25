/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LLM Agent Service - Main entry point for LLM-powered browser automation
 *
 * This module provides the public API for interacting with LLM agents
 * that can control the browser using the WebScraper infrastructure.
 */

import { FLOORP_LLM_DEFAULT_MODELS } from "../../common/defines.ts";
import { OpenAIProvider } from "./providers/OpenAIProvider.ts";
import { AnthropicProvider } from "./providers/AnthropicProvider.ts";
import { OllamaProvider } from "./providers/OllamaProvider.ts";
import type { LLMProvider } from "./providers/Provider.ts";
import { ToolRegistry } from "./tools/ToolRegistry.ts";
import {
  AgentContext,
  createBrowserAutomationContext,
} from "./context/AgentContext.ts";
import type {
  Message,
  CompletionOptions,
  CompletionResponse,
  StreamingCallback,
  ToolCall,
  ToolResult,
  LLMProviderConfig,
  AgentSession,
} from "./types.ts";
import { PREF_KEYS } from "./types.ts";

const DEFAULT_MAX_TOOL_ITERATIONS = 8;

interface AgentSessionRecord {
  session: AgentSession;
  context: AgentContext;
  providerName: string;
  maxToolIterations: number;
}

/**
 * LLM Agent Service
 *
 * Manages provider instances, tool registry, and agent sessions
 */
export class LLMAgentService {
  private providers: Map<string, LLMProvider> = new Map();
  private toolRegistry: ToolRegistry;
  private sessions: Map<string, AgentSessionRecord> = new Map();
  private prefs: nsIPrefBranch;

  constructor() {
    this.toolRegistry = new ToolRegistry();
    this.prefs = Services.prefs;
    this.initializeProviders();
  }

  /**
   * Initialize providers from preferences
   */
  private initializeProviders(): void {
    // OpenAI
    const openaiKey = this.getStringPref(PREF_KEYS.OPENAI_API_KEY, "");
    const openaiModel = this.getStringPref(
      PREF_KEYS.OPENAI_MODEL,
      FLOORP_LLM_DEFAULT_MODELS.openai,
    );
    if (openaiKey) {
      this.providers.set(
        "openai",
        new OpenAIProvider({
          apiKey: openaiKey,
          model: openaiModel,
        }),
      );
    }

    // Anthropic
    const anthropicKey = this.getStringPref(PREF_KEYS.ANTHROPIC_API_KEY, "");
    const anthropicModel = this.getStringPref(
      PREF_KEYS.ANTHROPIC_MODEL,
      FLOORP_LLM_DEFAULT_MODELS.anthropic,
    );
    if (anthropicKey) {
      this.providers.set(
        "anthropic",
        new AnthropicProvider({
          apiKey: anthropicKey,
          model: anthropicModel,
        }),
      );
    }

    // Ollama (always available if server is running)
    const ollamaBaseUrl = this.getStringPref(
      PREF_KEYS.OLLAMA_BASE_URL,
      "http://localhost:11434/api",
    );
    const ollamaModel = this.getStringPref(
      PREF_KEYS.OLLAMA_MODEL,
      FLOORP_LLM_DEFAULT_MODELS.ollama,
    );
    this.providers.set(
      "ollama",
      new OllamaProvider({
        baseUrl: ollamaBaseUrl,
        model: ollamaModel,
      }),
    );
  }

  /**
   * Get available provider names
   */
  getAvailableProviders(): string[] {
    return Array.from(this.providers.entries())
      .filter(([_, provider]) => provider.isConfigured())
      .map(([name]) => name);
  }

  /**
   * Get a provider by name
   */
  getProvider(name: string): LLMProvider | undefined {
    return this.providers.get(name);
  }

  /**
   * Configure a provider
   */
  configureProvider(config: LLMProviderConfig): void {
    switch (config.type) {
      case "openai":
        this.providers.set("openai", new OpenAIProvider(config));
        if (config.apiKey) {
          this.setStringPref(PREF_KEYS.OPENAI_API_KEY, config.apiKey);
        }
        if (config.model) {
          this.setStringPref(PREF_KEYS.OPENAI_MODEL, config.model);
        }
        break;

      case "anthropic":
        this.providers.set("anthropic", new AnthropicProvider(config));
        if (config.apiKey) {
          this.setStringPref(PREF_KEYS.ANTHROPIC_API_KEY, config.apiKey);
        }
        if (config.model) {
          this.setStringPref(PREF_KEYS.ANTHROPIC_MODEL, config.model);
        }
        break;

      case "ollama":
        this.providers.set("ollama", new OllamaProvider(config));
        if (config.baseUrl) {
          this.setStringPref(PREF_KEYS.OLLAMA_BASE_URL, config.baseUrl);
        }
        if (config.model) {
          this.setStringPref(PREF_KEYS.OLLAMA_MODEL, config.model);
        }
        break;
    }
  }

  /**
   * Set WebScraper actor reference for tool execution
   */
  setWebScraperActor(actor: unknown): void {
    // Cast to WebScraperActorInterface
    this.toolRegistry.setWebScraperActor(
      actor as import("./tools/WebScraperTool.ts").WebScraperActorInterface,
    );
  }

  /**
   * Create a new agent session
   */
  createSession(options: {
    providerName: string;
    systemPrompt?: string;
    sessionId?: string;
    maxToolIterations?: number;
  }): string {
    const sessionId = options.sessionId ?? this.generateSessionId();
    const provider = this.providers.get(options.providerName);

    if (!provider) {
      throw new Error(
        `Provider '${options.providerName}' not found or not configured`,
      );
    }

    const context = options.systemPrompt
      ? new AgentContext({ systemPrompt: options.systemPrompt })
      : createBrowserAutomationContext();

    const session: AgentSession = {
      id: sessionId,
      provider: {
        type: options.providerName as "openai" | "anthropic" | "ollama",
        model: provider.defaultModel,
      },
      systemPrompt: context.getSystemPrompt(),
      messages: [],
      createdAt: Date.now(),
      updatedAt: Date.now(),
    };

    this.sessions.set(sessionId, {
      session,
      context,
      providerName: options.providerName,
      maxToolIterations:
        options.maxToolIterations ?? DEFAULT_MAX_TOOL_ITERATIONS,
    });

    return sessionId;
  }

  /**
   * Get session by ID
   */
  getSession(sessionId: string): AgentSession | undefined {
    const record = this.sessions.get(sessionId);
    if (!record) {
      return undefined;
    }

    return {
      ...record.session,
      systemPrompt: record.context.getSystemPrompt(),
      messages: record.context.getMessages(),
    };
  }

  /**
   * Get session context (internal)
   */
  private getSessionContext(sessionId: string): AgentContext | undefined {
    return this.sessions.get(sessionId)?.context;
  }

  /**
   * Get session provider name (internal)
   */
  private getSessionProviderName(sessionId: string): string | undefined {
    return this.sessions.get(sessionId)?.providerName;
  }

  /**
   * Delete a session
   */
  deleteSession(sessionId: string): boolean {
    return this.sessions.delete(sessionId);
  }

  /**
   * Send a message and get a response
   */
  async chat(
    sessionId: string,
    userMessage: string,
    options?: {
      streaming?: boolean;
      onChunk?: StreamingCallback;
    },
  ): Promise<CompletionResponse> {
    const session = this.sessions.get(sessionId);
    if (!session) {
      throw new Error(`Session '${sessionId}' not found`);
    }

    const providerName = this.getSessionProviderName(sessionId);
    const context = this.getSessionContext(sessionId);

    if (!providerName || !context) {
      throw new Error(`Session '${sessionId}' is missing required data`);
    }

    const provider = this.providers.get(providerName);
    if (!provider) {
      throw new Error(`Provider '${providerName}' not found`);
    }

    // Add user message to context
    context.addUserMessage(userMessage);
    this.touchSession(sessionId);

    // Prepare completion options
    const completionOptions: CompletionOptions = {
      systemPrompt: context.getSystemPrompt(),
      tools: this.toolRegistry.getDefinitions(),
      maxTokens: 4096,
    };

    let response: CompletionResponse;

    if (options?.streaming && options.onChunk) {
      // Streaming mode
      let fullContent = "";
      const toolCalls: ToolCall[] = [];
      let finishReason: CompletionResponse["finishReason"] = "stop";

      await provider.completeStreaming(
        context.getMessages(),
        (chunk) => {
          options.onChunk!(chunk);
          if (chunk.type === "text") {
            fullContent += chunk.content ?? "";
          } else if (chunk.type === "tool_use" && chunk.toolCall) {
            const tc = chunk.toolCall;
            // Ensure toolCall has all required fields
            if (tc.id && tc.name && tc.arguments) {
              toolCalls.push(tc as ToolCall);
            }
          } else if (chunk.type === "done") {
            finishReason = chunk.finishReason ?? "stop";
          }
        },
        completionOptions,
      );

      response = {
        content: fullContent,
        toolCalls: toolCalls.length > 0 ? toolCalls : undefined,
        finishReason,
      };
    } else {
      // Non-streaming mode
      response = await provider.complete(
        context.getMessages(),
        completionOptions,
      );
    }

    // Add assistant response to context
    context.addAssistantMessage(response.content, response.toolCalls);
    this.touchSession(sessionId);

    // Handle tool calls
    if (response.toolCalls && response.toolCalls.length > 0) {
      response = await this.executeToolCalls(sessionId, response.toolCalls);
    }

    return response;
  }

  /**
   * Execute tool calls and continue conversation
   */
  private async executeToolCalls(
    sessionId: string,
    toolCalls: ToolCall[],
  ): Promise<CompletionResponse> {
    const context = this.getSessionContext(sessionId);
    const providerName = this.getSessionProviderName(sessionId);
    const record = this.sessions.get(sessionId);
    if (!context || !providerName || !record) {
      throw new Error(`Session '${sessionId}' is missing required data`);
    }

    const provider = this.providers.get(providerName);
    if (!provider) {
      throw new Error(`Provider '${providerName}' not found`);
    }

    let pendingToolCalls = [...toolCalls];
    let iteration = 0;
    let lastResponse: CompletionResponse = {
      content: "",
      finishReason: "tool_use",
    };

    while (pendingToolCalls.length > 0) {
      iteration += 1;
      if (iteration > record.maxToolIterations) {
        throw new Error(
          `Maximum tool iterations exceeded (${record.maxToolIterations})`,
        );
      }

      const results: Array<{
        toolUseId: string;
        toolName: string;
        result: ToolResult;
      }> = [];

      for (const toolCall of pendingToolCalls) {
        const result = await this.toolRegistry.execute(
          toolCall.name,
          toolCall.arguments,
          undefined,
          toolCall.id,
        );
        results.push({
          toolUseId: toolCall.id,
          toolName: toolCall.name,
          result,
        });
      }

      context.addToolResults(results);
      this.touchSession(sessionId);

      lastResponse = await provider.complete(context.getMessages(), {
        systemPrompt: context.getSystemPrompt(),
        tools: this.toolRegistry.getDefinitions(),
      });

      context.addAssistantMessage(lastResponse.content, lastResponse.toolCalls);
      this.touchSession(sessionId);

      pendingToolCalls = lastResponse.toolCalls ?? [];
    }

    return lastResponse;
  }

  /**
   * Execute a single tool manually
   */
  async executeTool(
    name: string,
    params: Record<string, unknown>,
  ): Promise<ToolResult> {
    return await this.toolRegistry.execute(name, params);
  }

  /**
   * Get available tools
   */
  getAvailableTools() {
    return this.toolRegistry.getDefinitions();
  }

  /**
   * Generate a unique session ID
   */
  private generateSessionId(): string {
    return `session-${Date.now()}-${Math.random().toString(36).substring(2, 9)}`;
  }

  /**
   * Get string preference
   */
  private getStringPref(key: string, defaultValue: string): string {
    try {
      return this.prefs.getStringPref(key, defaultValue);
    } catch {
      return defaultValue;
    }
  }

  /**
   * Set string preference
   */
  private setStringPref(key: string, value: string): void {
    try {
      this.prefs.setStringPref(key, value);
    } catch {
      // Ignore errors
    }
  }

  /**
   * Update session metadata after context changes
   */
  private touchSession(sessionId: string): void {
    const record = this.sessions.get(sessionId);
    if (!record) {
      return;
    }

    record.session.updatedAt = Date.now();
    record.session.systemPrompt = record.context.getSystemPrompt();
    record.session.messages = record.context.getMessages();
  }
}

// Singleton instance
let serviceInstance: LLMAgentService | null = null;

/**
 * Get the LLM Agent Service singleton
 */
export function getLLMAgentService(): LLMAgentService {
  if (!serviceInstance) {
    serviceInstance = new LLMAgentService();
  }
  return serviceInstance;
}

/**
 * Initialize the service (called by OSGlue)
 */
export function initLLMAgentService(): LLMAgentService {
  return getLLMAgentService();
}

// Export types
export type {
  Message,
  ToolCall,
  ToolResult,
  CompletionResponse,
  StreamingCallback,
};
