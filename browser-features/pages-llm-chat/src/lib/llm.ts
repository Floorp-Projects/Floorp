import type { LLMProviderSettings, LLMRequest, Message } from "../types";
import type { ToolCall } from "./tools";
import { BROWSER_TOOLS, executeToolCall, supportsToolCalling } from "./tools";
import { FLOORP_LLM_DEFAULT_MODELS } from "../../../modules/common/defines.ts";
import { getRpc, isRpcAvailable } from "./rpc";

// Re-export types for convenience
export type { LLMProviderSettings } from "../types";

// Types matching settings page format
interface LLMProviderConfig {
  enabled: boolean;
  apiKey: string | null;
  defaultModel: string;
  baseUrl: string | null;
}

interface LLMProvidersFormData {
  providers: Record<string, LLMProviderConfig>;
  defaultProvider: string;
}

// Firefox preferences utilities
const PREF_KEY = "floorp.llm.providers";

function normalizeBaseUrl(baseUrl: string): string {
  return baseUrl.replace(/\/+$/, "");
}

function resolveChatBaseUrl(
  type: LLMProviderSettings["type"],
  configuredBaseUrl: string | null | undefined,
): string {
  const baseUrl = normalizeBaseUrl(
    configuredBaseUrl || getDefaultBaseUrl(type),
  );

  if (type !== "ollama") {
    return baseUrl;
  }

  if (baseUrl.endsWith("/v1")) {
    return baseUrl;
  }

  if (baseUrl.endsWith("/api")) {
    return `${baseUrl.slice(0, -4)}/v1`;
  }

  return `${baseUrl}/v1`;
}

function requiresApiKey(type: string): boolean {
  return type !== "ollama";
}

function hasUsableProviderConfig(
  type: string,
  config: LLMProviderConfig | undefined,
): config is LLMProviderConfig {
  if (!config || !config.enabled) {
    return false;
  }

  if (requiresApiKey(type) && !config.apiKey) {
    return false;
  }

  return true;
}

function isAgenticChatSupported(type: LLMProviderSettings["type"]): boolean {
  // This page currently sends OpenAI-compatible tool-calling requests.
  return type !== "anthropic" && type !== "anthropic-compatible";
}

function getDefaultBaseUrl(type: string): string {
  switch (type) {
    case "openai":
      return "https://api.openai.com/v1";
    case "anthropic":
      return "https://api.anthropic.com/v1";
    case "ollama":
      return "http://localhost:11434/v1";
    default:
      return "";
  }
}

function getDefaultSettings(): LLMProvidersFormData {
  return {
    providers: {
      openai: {
        enabled: true,
        apiKey: null,
        defaultModel: FLOORP_LLM_DEFAULT_MODELS.openai,
        baseUrl: null,
      },
      anthropic: {
        enabled: true,
        apiKey: null,
        defaultModel: FLOORP_LLM_DEFAULT_MODELS.anthropic,
        baseUrl: null,
      },
      ollama: {
        enabled: true,
        apiKey: null,
        defaultModel: FLOORP_LLM_DEFAULT_MODELS.ollama,
        baseUrl: "http://localhost:11434",
      },
      "openai-compatible": {
        enabled: true,
        apiKey: null,
        defaultModel: "",
        baseUrl: null,
      },
      "anthropic-compatible": {
        enabled: true,
        apiKey: null,
        defaultModel: "",
        baseUrl: null,
      },
    },
    defaultProvider: "openai",
  };
}

function normalizeProvidersSettings(
  value: Partial<LLMProvidersFormData>,
): LLMProvidersFormData {
  const defaults = getDefaultSettings();
  const providers: Record<string, LLMProviderConfig> = {
    ...defaults.providers,
  };
  for (const [type, config] of Object.entries(value.providers ?? {})) {
    providers[type] = {
      ...(defaults.providers[type] ?? defaults.providers["openai-compatible"]),
      ...config,
    };
  }

  return {
    providers,
    defaultProvider:
      typeof value.defaultProvider === "string" &&
      value.defaultProvider in providers
        ? value.defaultProvider
        : defaults.defaultProvider,
  };
}

async function getProvidersSettingsAsync(): Promise<LLMProvidersFormData> {
  if (!isRpcAvailable()) {
    return getDefaultSettings();
  }

  try {
    const rpc = getRpc();
    const result = await rpc.getStringPref(PREF_KEY);
    if (!result) {
      return getDefaultSettings();
    }
    return normalizeProvidersSettings(
      JSON.parse(result) as Partial<LLMProvidersFormData>,
    );
  } catch (e) {
    console.error("[LLM] Error getting pref:", e);
    return getDefaultSettings();
  }
}

export async function getProviderSettingsAsync(
  type: string,
): Promise<LLMProviderSettings | null> {
  const settings = await getProvidersSettingsAsync();
  const config = settings.providers[type];

  if (!config || !config.enabled) return null;

  return {
    type: type as LLMProviderSettings["type"],
    apiKey: config.apiKey || "",
    baseUrl: resolveChatBaseUrl(
      type as LLMProviderSettings["type"],
      config.baseUrl,
    ),
    defaultModel: config.defaultModel || "",
    enabled: config.enabled,
  };
}

export async function getFirstEnabledProviderAsync(): Promise<LLMProviderSettings | null> {
  const settings = await getProvidersSettingsAsync();

  // First try the default provider
  const defaultConfig = settings.providers[settings.defaultProvider];

  if (hasUsableProviderConfig(settings.defaultProvider, defaultConfig)) {
    return {
      type: settings.defaultProvider as LLMProviderSettings["type"],
      apiKey: defaultConfig.apiKey || "",
      baseUrl: resolveChatBaseUrl(
        settings.defaultProvider as LLMProviderSettings["type"],
        defaultConfig.baseUrl,
      ),
      defaultModel: defaultConfig.defaultModel || "",
      enabled: true,
    };
  }

  // Fall back to any enabled provider usable for chat (including Ollama)
  for (const [type, config] of Object.entries(settings.providers)) {
    if (hasUsableProviderConfig(type, config)) {
      return {
        type: type as LLMProviderSettings["type"],
        apiKey: config.apiKey || "",
        baseUrl: resolveChatBaseUrl(
          type as LLMProviderSettings["type"],
          config.baseUrl,
        ),
        defaultModel: config.defaultModel || "",
        enabled: true,
      };
    }
  }
  return null;
}

// Get all enabled providers usable for chat (includes Ollama)
export async function getAllEnabledProvidersAsync(): Promise<
  LLMProviderSettings[]
> {
  const settings = await getProvidersSettingsAsync();
  const providers: LLMProviderSettings[] = [];

  for (const [type, config] of Object.entries(settings.providers)) {
    if (hasUsableProviderConfig(type, config)) {
      providers.push({
        type: type as LLMProviderSettings["type"],
        apiKey: config.apiKey || "",
        baseUrl: resolveChatBaseUrl(
          type as LLMProviderSettings["type"],
          config.baseUrl,
        ),
        defaultModel: config.defaultModel || "",
        enabled: true,
      });
    }
  }

  // Sort: default provider first
  if (settings.defaultProvider) {
    const defaultIndex = providers.findIndex(
      (p) => p.type === settings.defaultProvider,
    );
    if (defaultIndex > 0) {
      const [defaultProvider] = providers.splice(defaultIndex, 1);
      providers.unshift(defaultProvider);
    }
  }

  return providers;
}

// Keep sync versions for backward compatibility (will return defaults)
export function getProviderSettings(_type: string): LLMProviderSettings | null {
  console.warn(
    "[LLM] Sync getProviderSettings called, returning null. Use getProviderSettingsAsync.",
  );
  return null;
}

export function getFirstEnabledProvider(): LLMProviderSettings | null {
  console.warn(
    "[LLM] Sync getFirstEnabledProvider called. Use getFirstEnabledProviderAsync.",
  );
  return null;
}

// OpenAI-compatible API call
export async function* streamChat(
  settings: LLMProviderSettings,
  messages: Message[],
  onChunk?: (chunk: string) => void,
): AsyncGenerator<string, void, unknown> {
  if (!isAgenticChatSupported(settings.type)) {
    throw new Error(
      `Provider '${settings.type}' is not yet supported by this chat page.`,
    );
  }

  const url = getApiUrl(settings);
  const headers = getHeaders(settings);

  const body: LLMRequest = {
    messages: messages.map((m) => ({
      role: m.role,
      content: m.content,
    })),
    model: settings.defaultModel,
    stream: true,
    max_tokens: 4096,
  };

  const response = await fetch(url, {
    method: "POST",
    headers,
    body: JSON.stringify(body),
  });

  if (!response.ok) {
    const error = await response.text();
    throw new Error(`API error: ${response.status} - ${error}`);
  }

  const reader = response.body?.getReader();
  if (!reader) throw new Error("No response body");

  const decoder = new TextDecoder();
  let buffer = "";

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    buffer += decoder.decode(value, { stream: true });
    const lines = buffer.split("\n");
    buffer = lines.pop() || "";

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed === "data: [DONE]") continue;
      if (!trimmed.startsWith("data: ")) continue;

      try {
        const json = JSON.parse(trimmed.slice(6));
        const content = extractContent(json, settings.type);
        if (content) {
          onChunk?.(content);
          yield content;
        }
      } catch {
        // Skip invalid JSON
      }
    }
  }
}

function getApiUrl(settings: LLMProviderSettings): string {
  const baseUrl = normalizeBaseUrl(settings.baseUrl);
  if (
    settings.type === "anthropic" ||
    settings.type === "anthropic-compatible"
  ) {
    return `${baseUrl}/messages`;
  }
  return `${baseUrl}/chat/completions`;
}

function getHeaders(settings: LLMProviderSettings): Record<string, string> {
  const headers: Record<string, string> = {
    "Content-Type": "application/json",
  };

  if (
    settings.type === "anthropic" ||
    settings.type === "anthropic-compatible"
  ) {
    headers["x-api-key"] = settings.apiKey;
    headers["anthropic-version"] = "2023-06-01";
    headers["anthropic-dangerous-direct-browser-access"] = "true";
  } else {
    headers["Authorization"] = `Bearer ${settings.apiKey}`;
  }

  return headers;
}

function extractContent(json: unknown, providerType: string): string {
  if (providerType === "anthropic" || providerType === "anthropic-compatible") {
    // Anthropic format
    const data = json as { delta?: { text?: string }; type?: string };
    if (data.type === "content_block_delta" && data.delta?.text) {
      return data.delta.text;
    }
    return "";
  }

  // OpenAI format
  const data = json as { choices?: Array<{ delta?: { content?: string } }> };
  return data.choices?.[0]?.delta?.content || "";
}

// ============================================================================
// Retry Utilities
// ============================================================================

interface RetryOptions {
  maxRetries?: number;
  initialDelayMs?: number;
  maxDelayMs?: number;
  backoffMultiplier?: number;
  onRetry?: (attempt: number, error: unknown) => void;
}

function isTransientApiError(error: unknown): boolean {
  if (error instanceof Error) {
    const message = error.message.toLowerCase();
    const statusMatch = message.match(/api error:\s*(\d+)/);
    if (statusMatch) {
      const status = Number.parseInt(statusMatch[1], 10);
      // Retry on 429 (rate limit), 500, 502, 503, 504
      return status === 429 || status >= 500;
    }
    // Retry on network errors
    return (
      message.includes("fetch") ||
      message.includes("network") ||
      message.includes("timeout") ||
      message.includes("econnrefused") ||
      message.includes("enotfound") ||
      message.includes("failed to fetch")
    );
  }
  if (typeof error === "string") {
    const message = error.toLowerCase();
    return (
      message.includes("fetch") ||
      message.includes("network") ||
      message.includes("timeout") ||
      message.includes("econnrefused") ||
      message.includes("enotfound")
    );
  }
  return false;
}

async function retryWithBackoff<T>(
  fn: () => Promise<T>,
  options: RetryOptions = {},
): Promise<T> {
  const {
    maxRetries = 3,
    initialDelayMs = 500,
    maxDelayMs = 10000,
    backoffMultiplier = 2,
    onRetry,
  } = options;

  let lastError: unknown;
  let delay = initialDelayMs;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await fn();
    } catch (error) {
      lastError = error;

      if (attempt === maxRetries || !isTransientApiError(error)) {
        throw error;
      }

      onRetry?.(attempt + 1, error);
      await new Promise((resolve) => globalThis.setTimeout(resolve, delay));
      delay = Math.min(delay * backoffMultiplier, maxDelayMs);
    }
  }

  throw lastError;
}

export async function sendMessage(
  settings: LLMProviderSettings,
  messages: Message[],
): Promise<string> {
  return retryWithBackoff(
    async () => {
      let fullResponse = "";
      for await (const chunk of streamChat(settings, messages)) {
        fullResponse += chunk;
      }
      return fullResponse;
    },
    {
      maxRetries: 3,
      initialDelayMs: 500,
      onRetry: (attempt, error) => {
        console.warn(`[LLM] sendMessage retry ${attempt}:`, error);
      },
    },
  );
}

interface LLMRequestWithTools extends LLMRequest {
  tools?: typeof BROWSER_TOOLS;
}

interface ToolCallResponse {
  id: string;
  type: "function";
  function: {
    name: string;
    arguments: string;
  };
  index?: number;
}

export interface AgenticLoopCallbacks {
  onContent: (content: string) => void;
  onToolCallStart?: (event: {
    id: string;
    name: string;
    args: unknown;
    iteration: number;
  }) => void;
  onToolCallEnd?: (event: {
    id: string;
    name: string;
    args: unknown;
    result: string;
    iteration: number;
    isError: boolean;
  }) => void;
}

// Non-streaming request with tool support
export async function sendMessagesWithTools(
  settings: LLMProviderSettings,
  messages: Message[],
  _onToolCall?: (toolName: string, args: unknown) => void,
): Promise<{ content: string; toolCalls: ToolCallResponse[] }> {
  if (!supportsToolCalling(settings)) {
    throw new Error(
      `Provider '${settings.type}' tool calling is not supported in pages-llm-chat.`,
    );
  }

  const url = getApiUrl(settings);
  const headers = getHeaders(settings);

  const body: LLMRequestWithTools = {
    messages: messages.map((m) => ({
      role: m.role,
      content: m.content,
      // Include tool_calls for assistant messages
      ...(m.tool_calls ? { tool_calls: m.tool_calls } : {}),
      // Include tool_call_id for tool messages
      ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
    })),
    model: settings.defaultModel,
    stream: false,
    max_tokens: 4096,
  };

  // Add tools if supported
  if (supportsToolCalling(settings)) {
    body.tools = BROWSER_TOOLS;
  }

  const response = await fetch(url, {
    method: "POST",
    headers,
    body: JSON.stringify(body),
  });

  if (!response.ok) {
    const error = await response.text();
    throw new Error(`API error: ${response.status} - ${error}`);
  }

  const data = await response.json();

  // Extract content and tool calls
  const choice = data.choices?.[0];
  const content = choice?.message?.content || "";
  const toolCalls = choice?.message?.tool_calls || [];

  return { content, toolCalls };
}

// Run agentic loop with tools
export async function runAgenticLoop(
  settings: LLMProviderSettings,
  messages: Message[],
  callbacks: AgenticLoopCallbacks,
  maxIterations: number = 5,
): Promise<Message[]> {
  if (!supportsToolCalling(settings)) {
    throw new Error(
      `Provider '${settings.type}' is not supported in the current chat UI yet.`,
    );
  }

  const conversationMessages = [...messages];
  let iterations = 0;

  while (iterations < maxIterations) {
    iterations++;

    const { content, toolCalls } = await sendMessagesWithTools(
      settings,
      conversationMessages,
    );

    // If there's content, notify
    if (content) {
      callbacks.onContent(content);
    }

    // If no tool calls, we're done
    if (!toolCalls || toolCalls.length === 0) {
      // Add assistant message if not already added
      if (content) {
        conversationMessages.push({
          role: "assistant",
          content,
        });
      }
      break;
    }

    // Add assistant message with tool calls
    conversationMessages.push({
      role: "assistant",
      content: content || null,
      tool_calls: toolCalls,
    } as Message);

    // Execute each tool call
    for (const toolCall of toolCalls) {
      const toolName = toolCall.function.name;
      let args: unknown;
      try {
        args = JSON.parse(toolCall.function.arguments);
      } catch {
        args = { _raw: toolCall.function.arguments };
      }

      callbacks.onToolCallStart?.({
        id: toolCall.id,
        name: toolName,
        args,
        iteration: iterations,
      });

      // Execute the tool
      const result = await executeToolCall(toolCall as ToolCall);
      const isError =
        result.startsWith("Error executing ") || result.startsWith("Failed ");

      callbacks.onToolCallEnd?.({
        id: toolCall.id,
        name: toolName,
        args,
        result,
        iteration: iterations,
        isError,
      });

      // Add tool result message
      conversationMessages.push({
        role: "tool",
        content: result,
        tool_call_id: toolCall.id,
      } as Message);
    }
  }

  return conversationMessages;
}

// ============================================================================
// Streaming with Tools Support
// ============================================================================

interface StreamMessagesWithToolsCallbacks {
  onChunk?: (chunk: string) => void;
}

// Streaming request with tool support
// Streams content chunks via onChunk callback
// Returns tool_calls when stream completes
async function streamMessagesWithTools(
  settings: LLMProviderSettings,
  messages: Message[],
  callbacks: StreamMessagesWithToolsCallbacks = {},
): Promise<{ content: string; toolCalls: ToolCallResponse[] }> {
  if (!supportsToolCalling(settings)) {
    throw new Error(
      `Provider '${settings.type}' tool calling is not supported in pages-llm-chat.`,
    );
  }

  const url = getApiUrl(settings);
  const headers = getHeaders(settings);

  const body: LLMRequestWithTools = {
    messages: messages.map((m) => ({
      role: m.role,
      content: m.content,
      ...(m.tool_calls ? { tool_calls: m.tool_calls } : {}),
      ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
    })),
    model: settings.defaultModel,
    stream: true, // Enable streaming
    max_tokens: 4096,
  };

  if (supportsToolCalling(settings)) {
    body.tools = BROWSER_TOOLS;
  }

  const response = await retryWithBackoff(
    async () => {
      const res = await fetch(url, {
        method: "POST",
        headers,
        body: JSON.stringify(body),
      });

      if (!res.ok) {
        const error = await res.text();
        throw new Error(`API error: ${res.status} - ${error}`);
      }

      return res;
    },
    {
      maxRetries: 3,
      initialDelayMs: 500,
      onRetry: (attempt, error) => {
        console.warn(`[LLM] streamMessagesWithTools retry ${attempt}:`, error);
      },
    },
  );

  // Parse streaming response
  const reader = response.body?.getReader();
  if (!reader) throw new Error("No response body");

  const decoder = new TextDecoder();
  let buffer = "";
  let accumulatedContent = "";
  const toolCalls: ToolCallResponse[] = [];

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    buffer += decoder.decode(value, { stream: true });
    const lines = buffer.split("\n");
    buffer = lines.pop() || "";

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed === "data: [DONE]") continue;
      if (!trimmed.startsWith("data: ")) continue;

      try {
        const json = JSON.parse(trimmed.slice(6));

        // Extract content chunk
        const content = extractContent(json, settings.type);
        if (content) {
          accumulatedContent += content;
          callbacks.onChunk?.(content);
        }

        // Extract tool calls (OpenAI format)
        if (json.choices?.[0]?.delta?.tool_calls) {
          const deltaToolCalls = json.choices[0].delta.tool_calls as Array<{
            index?: number;
            id?: string;
            function?: {
              name?: string;
              arguments?: string;
            };
          }>;
          for (const deltaTool of deltaToolCalls) {
            const index = deltaTool.index ?? 0;
            // Find or create tool call entry
            let existingTool = toolCalls.find((t) => t.index === index);
            if (!existingTool) {
              existingTool = {
                id: deltaTool.id || crypto.randomUUID(),
                type: "function",
                function: {
                  name: deltaTool.function?.name || "",
                  arguments: deltaTool.function?.arguments || "",
                },
                index,
              } as ToolCallResponse & { index: number };
              toolCalls.push(existingTool);
            } else if (existingTool.function && deltaTool.function) {
              if (deltaTool.function.name) {
                existingTool.function.name += deltaTool.function.name;
              }
              if (deltaTool.function.arguments) {
                existingTool.function.arguments += deltaTool.function.arguments;
              }
            }
          }
        }
      } catch {
        // Skip invalid JSON
      }
    }
  }

  return { content: accumulatedContent, toolCalls };
}

// Extended callbacks with streaming support
export interface StreamAgenticLoopCallbacks extends AgenticLoopCallbacks {
  onStreamStart?: () => void;
  onStreamEnd?: () => void;
  onError?: (error: Error) => void;
}

// Run agentic loop with streaming support
export async function runAgenticLoopWithStream(
  settings: LLMProviderSettings,
  messages: Message[],
  callbacks: StreamAgenticLoopCallbacks,
  maxIterations: number = 5,
): Promise<Message[]> {
  if (!supportsToolCalling(settings)) {
    throw new Error(
      `Provider '${settings.type}' is not supported in the current chat UI yet.`,
    );
  }

  const conversationMessages = [...messages];
  let iterations = 0;

  while (iterations < maxIterations) {
    iterations++;

    callbacks.onStreamStart?.();

    let accumulatedContent = "";

    try {
      const { toolCalls } = await streamMessagesWithTools(
        settings,
        conversationMessages,
        {
          onChunk: (chunk) => {
            accumulatedContent += chunk;
            callbacks.onContent(accumulatedContent); // Send accumulated content
          },
        },
      );

      callbacks.onStreamEnd?.();

      // If no tool calls, we're done
      if (!toolCalls || toolCalls.length === 0) {
        if (accumulatedContent) {
          conversationMessages.push({
            role: "assistant",
            content: accumulatedContent,
          });
        }
        break;
      }

      // Add assistant message with tool calls
      conversationMessages.push({
        role: "assistant",
        content: accumulatedContent || null,
        tool_calls: toolCalls,
      } as Message);

      // Execute each tool call
      for (const toolCall of toolCalls) {
        const toolName = toolCall.function.name;
        let args: unknown;
        try {
          args = JSON.parse(toolCall.function.arguments);
        } catch {
          args = { _raw: toolCall.function.arguments };
        }

        callbacks.onToolCallStart?.({
          id: toolCall.id,
          name: toolName,
          args,
          iteration: iterations,
        });

        // Execute the tool with retry for transient errors
        let result = "";
        let attempts = 0;
        const maxToolAttempts = 2;

        while (attempts <= maxToolAttempts) {
          try {
            result = await executeToolCall(toolCall as ToolCall);
            break;
          } catch (error) {
            attempts++;
            if (attempts > maxToolAttempts) {
              result = `Error after ${maxToolAttempts} retries: ${error}`;
            } else {
              // Wait before retry
              await new Promise((resolve) =>
                globalThis.setTimeout(resolve, 300),
              );
            }
          }
        }

        const isError =
          result.startsWith("Error executing ") ||
          result.startsWith("Failed ") ||
          result.startsWith("Error after");

        callbacks.onToolCallEnd?.({
          id: toolCall.id,
          name: toolName,
          args,
          result,
          iteration: iterations,
          isError,
        });

        // Add tool result message
        conversationMessages.push({
          role: "tool",
          content: result,
          tool_call_id: toolCall.id,
        } as Message);
      }
    } catch (error) {
      callbacks.onError?.(error as Error);
      throw error;
    }
  }

  return conversationMessages;
}
