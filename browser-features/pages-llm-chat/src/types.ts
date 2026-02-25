export type LLMProviderType =
  | "openai"
  | "anthropic"
  | "ollama"
  | "openai-compatible"
  | "anthropic-compatible";

export interface LLMProviderSettings {
  type: LLMProviderType;
  apiKey: string;
  baseUrl: string;
  defaultModel: string;
  enabled: boolean;
}

export type ToolRunStatus = "running" | "completed" | "error";

export interface ToolRun {
  id: string;
  name: string;
  args: unknown;
  status: ToolRunStatus;
  startedAt: number;
  finishedAt?: number;
  resultPreview?: string;
}

export interface Message {
  id: string;
  role: "user" | "assistant" | "system" | "tool";
  content: string | null;
  timestamp: number;
  tool_calls?: unknown[];
  tool_call_id?: string;
  provider?: LLMProviderType;
  model?: string;
  toolRuns?: ToolRun[];
}

export interface ChatSession {
  id: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
}

export interface LLMRequest {
  messages: Array<{
    role: "user" | "assistant" | "system";
    content: string;
  }>;
  model: string;
  stream?: boolean;
  max_tokens?: number;
  temperature?: number;
}
