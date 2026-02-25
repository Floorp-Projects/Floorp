/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Agent Context for managing conversation state and message history
 */

import type { Message, ToolCall, ToolResult, ContentBlock } from "../types.ts";

/**
 * Configuration for AgentContext
 */
export interface AgentContextConfig {
  /** Maximum number of messages to keep in history */
  maxMessages?: number;
  /** System prompt to prepend to messages */
  systemPrompt?: string;
  /** Initial messages */
  initialMessages?: Message[];
}

/**
 * Manages conversation context and message history
 */
export class AgentContext {
  private messages: Message[] = [];
  private maxMessages: number;
  private systemPrompt?: string;

  constructor(config: AgentContextConfig = {}) {
    this.maxMessages = config.maxMessages ?? 100;
    this.systemPrompt = config.systemPrompt;
    if (config.initialMessages) {
      this.messages = [...config.initialMessages];
    }
  }

  /**
   * Get all messages in the context
   */
  getMessages(): Message[] {
    return [...this.messages];
  }

  /**
   * Get the system prompt
   */
  getSystemPrompt(): string | undefined {
    return this.systemPrompt;
  }

  /**
   * Set the system prompt
   */
  setSystemPrompt(prompt: string): void {
    this.systemPrompt = prompt;
  }

  /**
   * Add a user message
   */
  addUserMessage(content: string): void {
    this.addMessage({ role: "user", content });
  }

  /**
   * Add an assistant message
   */
  addAssistantMessage(content: string, toolCalls?: ToolCall[]): void {
    const message: Message = { role: "assistant", content };
    if (toolCalls && toolCalls.length > 0) {
      message.toolCalls = toolCalls;
    }
    this.addMessage(message);
  }

  /**
   * Add a tool result message
   */
  addToolResult(
    toolUseId: string,
    _toolName: string,
    result: ToolResult,
  ): void {
    const content: ContentBlock[] = [
      {
        type: "tool_result",
        toolUseId,
        content: result.result,
        isError: result.isError,
      },
    ];

    this.addMessage({
      role: "user",
      content,
    });
  }

  /**
   * Add multiple tool results
   */
  addToolResults(
    results: Array<{ toolUseId: string; toolName: string; result: ToolResult }>,
  ): void {
    const content: ContentBlock[] = results.map(({ toolUseId, result }) => ({
      type: "tool_result" as const,
      toolUseId,
      content: result.result,
      isError: result.isError,
    }));

    this.addMessage({
      role: "user",
      content,
    });
  }

  /**
   * Add a message to the context
   */
  addMessage(message: Message): void {
    this.messages.push(message);

    // Trim history if exceeds max
    if (this.messages.length > this.maxMessages) {
      // Keep at least the first message (often contains important context)
      const keepFirst = this.messages.length > 1;
      const excess = this.messages.length - this.maxMessages;
      const startIndex = keepFirst ? 1 : 0;
      this.messages.splice(startIndex, excess);
    }
  }

  /**
   * Clear all messages
   */
  clear(): void {
    this.messages = [];
  }

  /**
   * Get the last message
   */
  getLastMessage(): Message | undefined {
    return this.messages[this.messages.length - 1];
  }

  /**
   * Get message count
   */
  getMessageCount(): number {
    return this.messages.length;
  }

  /**
   * Check if context is empty
   */
  isEmpty(): boolean {
    return this.messages.length === 0;
  }

  /**
   * Export context for serialization
   */
  export(): { messages: Message[]; systemPrompt?: string } {
    return {
      messages: this.getMessages(),
      systemPrompt: this.systemPrompt,
    };
  }

  /**
   * Import context from serialization
   */
  import(data: { messages?: Message[]; systemPrompt?: string }): void {
    if (data.messages) {
      this.messages = data.messages;
    }
    if (data.systemPrompt !== undefined) {
      this.systemPrompt = data.systemPrompt;
    }
  }

  /**
   * Create a summary of the context for display
   */
  getSummary(): string {
    const parts: string[] = [];

    if (this.systemPrompt) {
      parts.push(`System: ${this.systemPrompt.substring(0, 100)}...`);
    }

    for (let i = 0; i < Math.min(this.messages.length, 5); i++) {
      const msg = this.messages[i];
      const content =
        typeof msg.content === "string"
          ? msg.content.substring(0, 50)
          : `[${msg.content.length} blocks]`;
      parts.push(`${msg.role}: ${content}...`);
    }

    if (this.messages.length > 5) {
      parts.push(`... and ${this.messages.length - 5} more messages`);
    }

    return parts.join("\n");
  }

  /**
   * Estimate token count (rough approximation)
   */
  estimateTokenCount(): number {
    let count = 0;

    if (this.systemPrompt) {
      count += this.estimateTokens(this.systemPrompt);
    }

    for (const msg of this.messages) {
      count += 4; // Role overhead

      if (typeof msg.content === "string") {
        count += this.estimateTokens(msg.content);
      } else {
        for (const block of msg.content) {
          if (block.type === "text") {
            count += this.estimateTokens(block.text);
          } else if (block.type === "tool_result") {
            count += this.estimateTokens(block.content);
          }
        }
      }

      if (msg.toolCalls) {
        for (const tc of msg.toolCalls) {
          count += this.estimateTokens(tc.name);
          count += this.estimateTokens(JSON.stringify(tc.arguments));
        }
      }
    }

    return count;
  }

  /**
   * Rough token estimation (1 token ≈ 4 characters for English)
   */
  private estimateTokens(text: string): number {
    return Math.ceil(text.length / 4);
  }

  /**
   * Clone the context
   */
  clone(): AgentContext {
    const cloned = new AgentContext({
      maxMessages: this.maxMessages,
      systemPrompt: this.systemPrompt,
      initialMessages: this.getMessages(),
    });
    return cloned;
  }
}

/**
 * Create a context with a default system prompt for browser automation
 */
export function createBrowserAutomationContext(): AgentContext {
  return new AgentContext({
    systemPrompt: `You are a browser automation assistant. You can control the web browser using the available tools.

When asked to perform tasks on web pages:
1. First understand the current page state using browser_control with action "read_page" or "get_page_info"
2. Use appropriate actions to interact with elements
3. Prefer CSS selectors to identify elements (XPath is not supported in the current browser_control implementation)
4. If an action fails, try alternative approaches
5. Report back what you found or accomplished

Available actions for browser_control tool:
- read_page: Get the full page content and structure
- get_page_info: Get basic page metadata (title, URL)
- find_element: Check if an element exists
- get_text: Get text content from an element
- click_element: Click on an element
- fill_input: Fill a form input
- scroll: Scroll the page
- wait_for_element: Wait for an element to appear
- wait_for_navigation: Wait for page navigation
- take_screenshot: Capture a screenshot
- highlight_element: Highlight an element visually
- clear_highlight: Remove element highlights

Be precise and efficient in your actions.`,
    maxMessages: 50,
  });
}
