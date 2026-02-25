/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OpenAI provider implementation
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

interface OpenAIMessage {
  role: "system" | "user" | "assistant" | "tool";
  content: string | null;
  name?: string;
  tool_call_id?: string;
  tool_calls?: OpenAIToolCall[];
}

interface OpenAIToolCall {
  id: string;
  type: "function";
  function: {
    name: string;
    arguments: string;
  };
}

interface OpenAITool {
  type: "function";
  function: {
    name: string;
    description: string;
    parameters: Record<string, unknown>;
  };
}

interface OpenAIResponse {
  id: string;
  object: string;
  created: number;
  model: string;
  choices: Array<{
    index: number;
    message: {
      role: string;
      content: string | null;
      tool_calls?: OpenAIToolCall[];
    };
    finish_reason: string;
  }>;
  usage?: {
    prompt_tokens: number;
    completion_tokens: number;
    total_tokens: number;
  };
}

export class OpenAIProvider extends BaseLLMProvider {
  readonly type = "openai" as const;
  readonly defaultModel = FLOORP_LLM_DEFAULT_MODELS.openai;

  private readonly apiBaseUrl: string;

  constructor(config: {
    apiKey?: string;
    baseUrl?: string;
    model?: string;
    timeout?: number;
  }) {
    super(config);
    this.apiBaseUrl = config.baseUrl ?? "https://api.openai.com/v1";
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
    const url = `${this.apiBaseUrl}/chat/completions`;
    const formattedMessages = this.formatMessages(
      messages,
      options?.systemPrompt,
    );

    const body: Record<string, unknown> = {
      model: this.model,
      messages: formattedMessages,
      max_tokens: options?.maxTokens ?? 4096,
      temperature: options?.temperature ?? 0.7,
    };

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
      body.tool_choice = "auto";
    }

    if (options?.stopSequences?.length) {
      body.stop = options.stopSequences;
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          ...this.getAuthHeader(),
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`OpenAI API error: ${response.status} - ${errorText}`);
    }

    const data = (await response.json()) as unknown as OpenAIResponse;
    const choice = data.choices[0];

    if (!choice) {
      throw new Error("No response from OpenAI");
    }

    const toolCalls: ToolCall[] | undefined = choice.message.tool_calls?.map(
      (tc): ToolCall => ({
        id: tc.id,
        name: tc.function.name,
        arguments: JSON.parse(tc.function.arguments),
      }),
    );

    return {
      content: choice.message.content ?? "",
      toolCalls,
      usage: data.usage
        ? {
            promptTokens: data.usage.prompt_tokens,
            completionTokens: data.usage.completion_tokens,
            totalTokens: data.usage.total_tokens,
          }
        : undefined,
      finishReason: this.mapFinishReason(choice.finish_reason),
      raw: data,
    };
  }

  async completeStreaming(
    messages: Message[],
    onChunk: StreamingCallback,
    options?: CompletionOptions,
  ): Promise<void> {
    const url = `${this.apiBaseUrl}/chat/completions`;
    const formattedMessages = this.formatMessages(
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

    if (options?.tools?.length) {
      body.tools = this.formatTools(options.tools);
      body.tool_choice = "auto";
    }

    const response = await this.fetchWithTimeout(
      url,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          ...this.getAuthHeader(),
        },
        body: JSON.stringify(body),
      },
      this.timeout,
    );

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`OpenAI API error: ${response.status} - ${errorText}`);
    }

    const reader = response.body?.getReader();
    if (!reader) {
      throw new Error("No response body");
    }

    const decoder = new TextDecoder("utf-8");
    let buffer = "";
    const currentToolCalls: Map<
      string,
      { id: string; name: string; args: string }
    > = new Map();

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
          if (data === "[DONE]") {
            onChunk({ type: "done" });
            return;
          }

          try {
            const parsed = JSON.parse(data);
            const delta = parsed.choices?.[0]?.delta;
            const finishReason = parsed.choices?.[0]?.finish_reason;

            if (delta?.content) {
              onChunk({ type: "text", content: delta.content });
            }

            if (delta?.tool_calls) {
              for (const tc of delta.tool_calls) {
                const id = tc.id;
                const name = tc.function?.name;
                const args = tc.function?.arguments ?? "";

                if (id && name) {
                  currentToolCalls.set(id, { id, name, args });
                } else if (
                  tc.index !== undefined &&
                  currentToolCalls.size > 0
                ) {
                  const existing = Array.from(currentToolCalls.values())[
                    tc.index
                  ];
                  if (existing) {
                    existing.args += args;
                  }
                }
              }
            }

            if (finishReason === "tool_calls" && currentToolCalls.size > 0) {
              for (const tc of currentToolCalls.values()) {
                try {
                  onChunk({
                    type: "tool_use",
                    toolCall: {
                      id: tc.id,
                      name: tc.name,
                      arguments: JSON.parse(tc.args),
                    },
                  });
                } catch {
                  // Invalid JSON, skip
                }
              }
              currentToolCalls.clear();
            }

            if (finishReason) {
              onChunk({
                type: "done",
                finishReason: this.mapFinishReason(finishReason),
              });
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
  ): OpenAIMessage[] {
    const result: OpenAIMessage[] = [];

    if (systemPrompt) {
      result.push({ role: "system", content: systemPrompt });
    }

    for (const msg of messages) {
      if (typeof msg.content === "string") {
        if (msg.role === "assistant" && msg.toolCalls?.length) {
          result.push({
            role: "assistant",
            content: msg.content || null,
            tool_calls: this.serializeToolCalls(msg.toolCalls),
          });
        } else {
          result.push({ role: msg.role, content: msg.content });
        }
      } else {
        // Handle complex content
        const textParts: string[] = [];
        for (const part of msg.content) {
          if (part.type === "text") {
            textParts.push(part.text);
          } else if (part.type === "tool_result") {
            result.push({
              role: "tool",
              content: part.content,
              tool_call_id: part.toolUseId,
            });
          }
        }
        if (
          textParts.length > 0 ||
          (msg.role === "assistant" && msg.toolCalls?.length)
        ) {
          result.push({
            role: msg.role,
            content: textParts.length > 0 ? textParts.join("\n") : null,
            ...(msg.role === "assistant" && msg.toolCalls?.length
              ? { tool_calls: this.serializeToolCalls(msg.toolCalls) }
              : {}),
          });
        }
      }
    }

    return result;
  }

  private serializeToolCalls(toolCalls: ToolCall[]): OpenAIToolCall[] {
    return toolCalls.map((toolCall) => ({
      id: toolCall.id,
      type: "function",
      function: {
        name: toolCall.name,
        arguments: JSON.stringify(toolCall.arguments),
      },
    }));
  }

  protected override formatTools(tools: ToolDefinition[]): OpenAITool[] {
    return tools.map((tool) => ({
      type: "function" as const,
      function: {
        name: tool.name,
        description: tool.description,
        parameters: tool.parameters as Record<string, unknown>,
      },
    }));
  }

  private mapFinishReason(reason: string): CompletionResponse["finishReason"] {
    switch (reason) {
      case "stop":
        return "stop";
      case "tool_calls":
        return "tool_use";
      case "length":
        return "length";
      default:
        return "stop";
    }
  }
}
