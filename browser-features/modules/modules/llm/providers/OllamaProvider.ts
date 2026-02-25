/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Ollama provider implementation for local LLM support
 */

import { FLOORP_LLM_DEFAULT_MODELS } from "../../../common/defines.ts";
import { BaseLLMProvider } from "./Provider.ts";
import type {
  Message,
  CompletionOptions,
  CompletionResponse,
  StreamingCallback,
  ToolDefinition,
  ToolCall,
} from "../types.ts";

interface OllamaMessage {
  role: "system" | "user" | "assistant" | "tool";
  content: string;
  images?: string[];
  tool_call_id?: string;
  tool_calls?: Array<{
    function: {
      name: string;
      arguments: Record<string, unknown>;
    };
  }>;
}

interface OllamaTool {
  type: "function";
  function: {
    name: string;
    description: string;
    parameters: Record<string, unknown>;
  };
}

interface OllamaResponse {
  model: string;
  created_at: string;
  message: {
    role: string;
    content: string;
    tool_calls?: Array<{
      function: {
        name: string;
        arguments: Record<string, unknown>;
      };
    }>;
  };
  done: boolean;
  total_duration?: number;
  load_duration?: number;
  prompt_eval_count?: number;
  eval_count?: number;
  eval_duration?: number;
}

export class OllamaProvider extends BaseLLMProvider {
  readonly type = "ollama" as const;
  readonly defaultModel = FLOORP_LLM_DEFAULT_MODELS.ollama;

  private readonly apiBaseUrl: string;

  constructor(config: {
    apiKey?: string;
    baseUrl?: string;
    model?: string;
    timeout?: number;
  }) {
    super(config);
    // Ollama typically runs locally on port 11434
    this.apiBaseUrl = config.baseUrl ?? "http://localhost:11434/api";
    // Set model to default if not provided in config
    if (!config.model) {
      this.model = this.defaultModel;
    }
  }

  isConfigured(): boolean {
    // Ollama doesn't require an API key for local usage
    return true;
  }

  async complete(
    messages: Message[],
    options?: CompletionOptions,
  ): Promise<CompletionResponse> {
    const url = `${this.apiBaseUrl}/chat`;
    const formattedMessages = this.formatMessages(
      messages,
      options?.systemPrompt,
    );

    const body: Record<string, unknown> = {
      model: this.model,
      messages: formattedMessages,
      stream: false,
      options: {
        num_ctx: options?.maxTokens ?? 4096,
        temperature: options?.temperature ?? 0.7,
      },
    };

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`Ollama API error: ${response.status} - ${errorText}`);
    }

    const data = (await response.json()) as unknown as OllamaResponse;

    const toolCalls: ToolCall[] | undefined = data.message.tool_calls?.map(
      (tc, index): ToolCall => ({
        id: `ollama-tool-${index}`,
        name: tc.function.name,
        arguments: tc.function.arguments,
      }),
    );

    return {
      content: data.message.content,
      toolCalls,
      usage:
        data.prompt_eval_count && data.eval_count
          ? {
              promptTokens: data.prompt_eval_count,
              completionTokens: data.eval_count,
              totalTokens: data.prompt_eval_count + data.eval_count,
            }
          : undefined,
      finishReason: data.done ? "stop" : "length",
      raw: data,
    };
  }

  async completeStreaming(
    messages: Message[],
    onChunk: StreamingCallback,
    options?: CompletionOptions,
  ): Promise<void> {
    const url = `${this.apiBaseUrl}/chat`;
    const formattedMessages = this.formatMessages(
      messages,
      options?.systemPrompt,
    );

    const body: Record<string, unknown> = {
      model: this.model,
      messages: formattedMessages,
      stream: true,
      options: {
        num_ctx: options?.maxTokens ?? 4096,
        temperature: options?.temperature ?? 0.7,
      },
    };

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`Ollama API error: ${response.status} - ${errorText}`);
    }

    const reader = response.body?.getReader();
    if (!reader) {
      throw new Error("No response body");
    }

    const decoder = new TextDecoder("utf-8");
    let buffer = "";

    try {
      while (true) {
        // @ts-expect-error Firefox ReadableStreamDefaultReader.read() doesn't require arguments
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split("\n");
        buffer = lines.pop() ?? "";

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed) continue;

          try {
            const data: OllamaResponse = JSON.parse(trimmed);

            if (data.message?.content) {
              onChunk({ type: "text", content: data.message.content });
            }

            if (data.message?.tool_calls) {
              for (let i = 0; i < data.message.tool_calls.length; i++) {
                const tc = data.message.tool_calls[i];
                onChunk({
                  type: "tool_use",
                  toolCall: {
                    id: `ollama-tool-${i}`,
                    name: tc.function.name,
                    arguments: tc.function.arguments,
                  },
                });
              }
            }

            if (data.done) {
              onChunk({ type: "done", finishReason: "stop" });
              return;
            }
          } catch {
            // Skip invalid JSON
          }
        }
      }
    } finally {
      reader.releaseLock();
    }
  }

  protected override formatMessages(
    messages: Message[],
    systemPrompt?: string,
  ): OllamaMessage[] {
    const result: OllamaMessage[] = [];

    if (systemPrompt) {
      result.push({ role: "system", content: systemPrompt });
    }

    for (const msg of messages) {
      if (typeof msg.content === "string") {
        const message: OllamaMessage = { role: msg.role, content: msg.content };
        if (msg.role === "assistant" && msg.toolCalls?.length) {
          message.tool_calls = this.serializeToolCalls(msg.toolCalls);
        }
        result.push(message);
      } else {
        const textParts: string[] = [];
        const images: string[] = [];

        for (const part of msg.content) {
          if (part.type === "text") {
            textParts.push(part.text);
          } else if (part.type === "image") {
            // Ollama accepts base64 images directly
            if (part.base64) {
              images.push(part.base64);
            }
          } else if (part.type === "tool_result") {
            result.push({
              role: "tool",
              content: part.content,
              tool_call_id: part.toolUseId,
            });
          }
        }

        if (textParts.length > 0 || images.length > 0) {
          const message: OllamaMessage = {
            role: msg.role,
            content: textParts.join("\n"),
          };
          if (msg.role === "assistant" && msg.toolCalls?.length) {
            message.tool_calls = this.serializeToolCalls(msg.toolCalls);
          }
          if (images.length > 0) {
            message.images = images;
          }
          result.push(message);
        } else if (msg.role === "assistant" && msg.toolCalls?.length) {
          result.push({
            role: "assistant",
            content: "",
            tool_calls: this.serializeToolCalls(msg.toolCalls),
          });
        }
      }
    }

    return result;
  }

  private serializeToolCalls(
    toolCalls: ToolCall[],
  ): NonNullable<OllamaMessage["tool_calls"]> {
    return toolCalls.map((toolCall) => ({
      function: {
        name: toolCall.name,
        arguments: toolCall.arguments,
      },
    }));
  }

  protected override formatTools(tools: ToolDefinition[]): OllamaTool[] {
    return tools.map((tool) => ({
      type: "function" as const,
      function: {
        name: tool.name,
        description: tool.description,
        parameters: tool.parameters as Record<string, unknown>,
      },
    }));
  }

  /**
   * Check if Ollama server is running
   */
  async isAvailable(): Promise<boolean> {
    try {
      const response = await this.fetchWithTimeout(
        `${this.apiBaseUrl.replace("/api", "")}/`,
        { method: "GET" },
        5000,
      );
      return response.ok;
    } catch {
      return false;
    }
  }

  /**
   * List available models
   */
  async listModels(): Promise<string[]> {
    const response = await this.fetchWithTimeout(
      `${this.apiBaseUrl.replace("/api", "")}/api/tags`,
      { method: "GET" },
      this.timeout,
    );

    if (!response.ok) {
      throw new Error(`Failed to list models: ${response.status}`);
    }

    const data = (await response.json()) as { models?: { name: string }[] };
    return data.models?.map((m) => m.name) ?? [];
  }

  /**
   * Pull a model from Ollama registry
   */
  async pullModel(modelName: string): Promise<void> {
    const response = await this.fetchWithTimeout(
      `${this.apiBaseUrl.replace("/api", "")}/api/pull`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: modelName, stream: false }),
      },
      300000, // 5 minutes timeout for model download
    );

    if (!response.ok) {
      throw new Error(`Failed to pull model: ${response.status}`);
    }
  }
}
