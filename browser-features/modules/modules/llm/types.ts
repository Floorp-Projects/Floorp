/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Core type definitions for the LLM Agent module
 */

// ============================================================================
// Message Types
// ============================================================================

export type MessageRole = "system" | "user" | "assistant" | "tool";

export interface TextContent {
  type: "text";
  text: string;
}

export interface ImageContent {
  type: "image";
  url: string;
  base64?: string;
  mimeType?: string;
}

export interface ToolUseContent {
  type: "tool_use";
  id: string;
  name: string;
  input: Record<string, unknown>;
}

export interface ToolResultContent {
  type: "tool_result";
  toolUseId: string;
  content: string;
  isError?: boolean;
}

export type MessageContent =
  | TextContent
  | ImageContent
  | ToolUseContent
  | ToolResultContent;

// ContentBlock is an alias for MessageContent
export type ContentBlock = MessageContent;

export interface Message {
  role: MessageRole;
  content: string | MessageContent[];
  name?: string;
  toolCalls?: ToolCall[];
}

export interface Conversation {
  id: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
  metadata?: Record<string, unknown>;
}

// ============================================================================
// Tool Types
// ============================================================================

export interface ToolParameter {
  type: "string" | "number" | "boolean" | "object" | "array";
  description?: string;
  enum?: string[];
  properties?: Record<string, ToolParameter>;
  required?: string[];
  items?: ToolParameter;
  default?: unknown;
}

export interface ToolDefinition {
  name: string;
  description: string;
  parameters: {
    type: "object";
    properties: Record<string, ToolParameter>;
    required?: string[];
  };
}

export interface ToolCall {
  id: string;
  name: string;
  arguments: Record<string, unknown>;
}

export interface ToolResult {
  /** Tool call ID - set by ToolRegistry when executing */
  toolCallId?: string;
  /** Result content as string */
  result: string;
  /** Whether the execution resulted in an error */
  isError?: boolean;
}

export interface ToolExecutor {
  definition: ToolDefinition;
  execute(args: Record<string, unknown>): Promise<ToolResult>;
}

// ============================================================================
// Provider Types
// ============================================================================

export type ProviderType = "openai" | "anthropic" | "ollama" | "custom";

export interface ProviderConfig {
  type: ProviderType;
  apiKey?: string;
  baseUrl?: string;
  model: string;
  maxTokens?: number;
  temperature?: number;
  timeout?: number;
}

export interface CompletionOptions {
  maxTokens?: number;
  temperature?: number;
  stopSequences?: string[];
  tools?: ToolDefinition[];
  systemPrompt?: string;
}

export interface CompletionResponse {
  content: string;
  toolCalls?: ToolCall[];
  usage?: {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
  };
  finishReason: "stop" | "tool_use" | "length" | "error";
  raw?: unknown;
}

export interface StreamingChunk {
  type: "text" | "tool_use" | "done" | "error";
  content?: string;
  toolCall?: Partial<ToolCall>;
  finishReason?: CompletionResponse["finishReason"];
  error?: string;
}

export type StreamingCallback = (chunk: StreamingChunk) => void;

// ============================================================================
// Agent Types
// ============================================================================

export interface AgentConfig {
  provider: ProviderConfig;
  systemPrompt?: string;
  tools?: ToolExecutor[];
  maxIterations?: number;
  onToolCall?: (call: ToolCall) => void;
  onToolResult?: (result: ToolResult) => void;
  onStreamingChunk?: StreamingCallback;
}

export interface AgentState {
  conversationId: string;
  isProcessing: boolean;
  currentToolCalls: ToolCall[];
  error?: string;
}

export type AgentEventHandler = {
  onMessage?: (message: Message) => void;
  onToolCall?: (call: ToolCall) => void;
  onToolResult?: (result: ToolResult) => void;
  onError?: (error: Error) => void;
  onStateChange?: (state: AgentState) => void;
};

// Session management
export interface AgentSession {
  id: string;
  provider: ProviderConfig;
  systemPrompt?: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
}

// Re-export as LLMProviderConfig for backward compatibility
export type LLMProviderConfig = ProviderConfig;

// ============================================================================
// API Key Management
// ============================================================================

export interface ApiKeyStorage {
  provider: ProviderType;
  key: string;
  storedAt: number;
}

// ============================================================================
// Preference Keys (for Firefox preferences)
// ============================================================================

export const PREF_KEYS = {
  ENABLED: "floorp.llm.enabled",
  PROVIDER: "floorp.llm.provider",
  API_KEY_PREFIX: "floorp.llm.apiKey.",
  MODEL: "floorp.llm.model",
  BASE_URL: "floorp.llm.baseUrl",
  MAX_TOKENS: "floorp.llm.maxTokens",
  TEMPERATURE: "floorp.llm.temperature",
  SYSTEM_PROMPT: "floorp.llm.systemPrompt",
  // Provider-specific keys
  OPENAI_API_KEY: "floorp.llm.apiKey.openai",
  OPENAI_MODEL: "floorp.llm.model.openai",
  ANTHROPIC_API_KEY: "floorp.llm.apiKey.anthropic",
  ANTHROPIC_MODEL: "floorp.llm.model.anthropic",
  OLLAMA_BASE_URL: "floorp.llm.baseUrl.ollama",
  OLLAMA_MODEL: "floorp.llm.model.ollama",
} as const;
