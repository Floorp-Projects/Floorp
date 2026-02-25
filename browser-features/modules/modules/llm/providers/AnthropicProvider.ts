/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Anthropic (Claude) provider implementation
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

interface AnthropicMessage {
  role: "user" | "assistant";
  content: string | AnthropicContentBlock[];
}

interface AnthropicContentBlock {
  type: "text" | "tool_use" | "tool_result";
  text?: string;
  id?: string;
  name?: string;
  input?: Record<string, unknown>;
  tool_use_id?: string;
  content?: string;
  is_error?: boolean;
}

interface AnthropicTool {
  name: string;
  description: string;
  input_schema: Record<string, unknown>;
}

interface AnthropicResponse {
  id: string;
  type: "message";
  role: "assistant";
  content: AnthropicContentBlock[];
  model: string;
  stop_reason: "end_turn" | "max_tokens" | "stop_sequence" | "tool_use" | null;
  stop_sequence: string | null;
  usage: {
    input_tokens: number;
    output_tokens: number;
  };
}

export class AnthropicProvider extends BaseLLMProvider {
  readonly type = "anthropic" as const;
  readonly defaultModel = FLOORP_LLM_DEFAULT_MODELS.anthropic;

  private readonly apiBaseUrl: string;

  constructor(config: {
    apiKey?: string;
    baseUrl?: string;
    model?: string;
    timeout?: number;
  }) {
    super(config);
    this.apiBaseUrl = config.baseUrl ?? "https://api.anthropic.com/v1";
    // Set model to default if not provided in config
    if (!config.model) {
      this.model = this.defaultModel;
    }
  }

  isConfigured(): boolean {
    return !!this.apiKey;
  }

  async complete(
    messages: Message[],
    options?: CompletionOptions,
  ): Promise<CompletionResponse> {
    const url = `${this.apiBaseUrl}/messages`;
    const { system, formattedMessages } = this.formatMessages(
      messages,
      options?.systemPrompt,
    );

    const body: Record<string, unknown> = {
      model: this.model,
      messages: formattedMessages,
      max_tokens: options?.maxTokens ?? 4096,
      temperature: options?.temperature ?? 0.7,
    };

    if (system) {
      body.system = system;
    }

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
    }

    if (options?.stopSequences?.length) {
      body.stop_sequences = options.stopSequences;
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "anthropic-version": "2023-06-01",
          ...this.getAuthHeader("x-api-key"),
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`Anthropic API error: ${response.status} - ${errorText}`);
    }

    const data = (await response.json()) as unknown as AnthropicResponse;

    const toolCalls: ToolCall[] = [];
    let textContent = "";

    for (const block of data.content) {
      if (block.type === "text") {
        textContent += block.text ?? "";
      } else if (block.type === "tool_use" && block.id && block.name) {
        toolCalls.push({
          id: block.id,
          name: block.name,
          arguments: block.input ?? {},
        });
      }
    }

    return {
      content: textContent,
      toolCalls: toolCalls.length > 0 ? toolCalls : undefined,
      usage: {
        promptTokens: data.usage.input_tokens,
        completionTokens: data.usage.output_tokens,
        totalTokens: data.usage.input_tokens + data.usage.output_tokens,
      },
      finishReason: this.mapStopReason(data.stop_reason),
      raw: data,
    };
  }

  async completeStreaming(
    messages: Message[],
    onChunk: StreamingCallback,
    options?: CompletionOptions,
  ): Promise<void> {
    const url = `${this.apiBaseUrl}/messages`;
    const { system, formattedMessages } = this.formatMessages(
      messages,
      options?.systemPrompt,
    );

    const body: Record<string, unknown> = {
      model: this.model,
      messages: formattedMessages,
      max_tokens: options?.maxTokens ?? 4096,
      temperature: options?.temperature ?? 0.7,
      stream: true,
    };

    if (system) {
      body.system = system;
    }

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "anthropic-version": "2023-06-01",
          ...this.getAuthHeader("x-api-key"),
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`Anthropic API error: ${response.status} - ${errorText}`);
    }

    const reader = response.body?.getReader();
    if (!reader) {
      throw new Error("No response body");
    }

    const decoder = new TextDecoder("utf-8");
    let buffer = "";
    let currentToolCall: { id: string; name: string; input: string } | null =
      null;

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
          if (!trimmed || !trimmed.startsWith("data: ")) continue;

          const data = trimmed.slice(6);
          try {
            const event = JSON.parse(data);
            const eventType = event.type;

            if (eventType === "content_block_delta") {
              const delta = event.delta;
              if (delta.type === "text_delta") {
                onChunk({ type: "text", content: delta.text });
              } else if (delta.type === "input_json_delta") {
                if (currentToolCall) {
                  currentToolCall.input += delta.partial_json;
                }
              }
            } else if (eventType === "content_block_start") {
              const block = event.content_block;
              if (block.type === "tool_use") {
                currentToolCall = {
                  id: block.id,
                  name: block.name,
                  input: "",
                };
              }
            } else if (eventType === "content_block_stop") {
              if (currentToolCall) {
                try {
                  const args = currentToolCall.input
                    ? JSON.parse(currentToolCall.input)
                    : {};
                  onChunk({
                    type: "tool_use",
                    toolCall: {
                      id: currentToolCall.id,
                      name: currentToolCall.name,
                      arguments: args,
                    },
                  });
                } catch {
                  // Invalid JSON, skip
                }
                currentToolCall = null;
              }
            } else if (eventType === "message_stop") {
              onChunk({ type: "done" });
              return;
            } else if (eventType === "message_delta") {
              if (event.delta?.stop_reason) {
                onChunk({
                  type: "done",
                  finishReason: this.mapStopReason(event.delta.stop_reason),
                });
              }
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
  ): { system?: string; formattedMessages: AnthropicMessage[] } {
    const result: AnthropicMessage[] = [];
    let system: string | undefined;

    if (systemPrompt) {
      system = systemPrompt;
    }

    for (const msg of messages) {
      // Skip system messages - they go in the system parameter
      if (msg.role === "system") continue;

      // Only user and assistant roles are valid for Anthropic
      if (msg.role !== "user" && msg.role !== "assistant") continue;

      const role = msg.role as "user" | "assistant";

      if (typeof msg.content === "string") {
        if (role === "assistant" && msg.toolCalls?.length) {
          const blocks: AnthropicContentBlock[] = [];
          if (msg.content) {
            blocks.push({ type: "text", text: msg.content });
          }
          blocks.push(...this.serializeToolUseBlocks(msg.toolCalls));
          result.push({ role, content: blocks });
        } else {
          result.push({ role, content: msg.content });
        }
      } else {
        const blocks: AnthropicContentBlock[] = [];
        for (const part of msg.content) {
          if (part.type === "text") {
            blocks.push({ type: "text", text: part.text });
          } else if (part.type === "tool_result") {
            blocks.push({
              type: "tool_result",
              tool_use_id: part.toolUseId,
              content: part.content,
              is_error: part.isError,
            });
          }
        }
        if (role === "assistant" && msg.toolCalls?.length) {
          blocks.push(...this.serializeToolUseBlocks(msg.toolCalls));
        }

        if (blocks.length === 0) {
          result.push({ role, content: "" });
        } else {
          result.push({ role, content: blocks });
        }
      }
    }

    return { system, formattedMessages: result };
  }

  private serializeToolUseBlocks(
    toolCalls: ToolCall[],
  ): AnthropicContentBlock[] {
    return toolCalls.map((toolCall) => ({
      type: "tool_use",
      id: toolCall.id,
      name: toolCall.name,
      input: toolCall.arguments,
    }));
  }

  protected override formatTools(tools: ToolDefinition[]): AnthropicTool[] {
    return tools.map((tool) => ({
      name: tool.name,
      description: tool.description,
      input_schema: tool.parameters as Record<string, unknown>,
    }));
  }

  private mapStopReason(
    reason: string | null,
  ): CompletionResponse["finishReason"] {
    switch (reason) {
      case "end_turn":
        return "stop";
      case "tool_use":
        return "tool_use";
      case "max_tokens":
        return "length";
      case "stop_sequence":
        return "stop";
      default:
        return "stop";
    }
  }
}
