/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Base Tool interface and abstract implementation
 */

import type { ToolDefinition, ToolResult } from "../types.ts";

/**
 * Interface for tools that can be used by the LLM Agent
 */
export interface Tool {
  /** Tool definition sent to LLM */
  readonly definition: ToolDefinition;

  /**
   * Execute the tool with given parameters
   * @param params - Parameters from LLM
   * @param context - Execution context (actor reference, etc.)
   */
  execute(
    params: Record<string, unknown>,
    context?: ToolContext,
  ): Promise<ToolResult>;
}

/**
 * Context passed to tool execution
 */
export interface ToolContext {
  /** Current browsing context ID */
  browsingContextId?: string;
  /** Actor reference for communicating with content process */
  actorRef?: unknown;
  /** Additional metadata */
  metadata?: Record<string, unknown>;
}

/**
 * Abstract base class for tools with common utilities
 */
export abstract class BaseTool implements Tool {
  abstract readonly definition: ToolDefinition;

  abstract execute(
    params: Record<string, unknown>,
    context?: ToolContext,
  ): Promise<ToolResult>;

  /**
   * Create a successful result
   * @param result - Result content (required)
   * @param toolCallId - Optional tool call ID (set by registry if not provided)
   */
  protected success(result: string, toolCallId?: string): ToolResult {
    const toolResult: ToolResult = {
      result,
      isError: false,
    };
    if (toolCallId) {
      toolResult.toolCallId = toolCallId;
    }
    return toolResult;
  }

  /**
   * Create an error result
   * @param message - Error message
   * @param toolCallId - Optional tool call ID (set by registry if not provided)
   */
  protected error(message: string, toolCallId?: string): ToolResult {
    const toolResult: ToolResult = {
      result: message,
      isError: true,
    };
    if (toolCallId) {
      toolResult.toolCallId = toolCallId;
    }
    return toolResult;
  }

  /**
   * Validate required parameters
   */
  protected validateParams(
    params: Record<string, unknown>,
    required: string[],
  ): string | null {
    for (const key of required) {
      if (
        !(key in params) ||
        params[key] === undefined ||
        params[key] === null
      ) {
        return `Missing required parameter: ${key}`;
      }
    }
    return null;
  }

  /**
   * Get string parameter with default
   */
  protected getString(
    params: Record<string, unknown>,
    key: string,
    defaultValue?: string,
  ): string {
    const value = params[key];
    if (typeof value === "string") return value;
    if (defaultValue !== undefined) return defaultValue;
    return "";
  }

  /**
   * Get number parameter with default
   */
  protected getNumber(
    params: Record<string, unknown>,
    key: string,
    defaultValue?: number,
  ): number {
    const value = params[key];
    if (typeof value === "number") return value;
    if (defaultValue !== undefined) return defaultValue;
    return 0;
  }

  /**
   * Get boolean parameter with default
   */
  protected getBoolean(
    params: Record<string, unknown>,
    key: string,
    defaultValue?: boolean,
  ): boolean {
    const value = params[key];
    if (typeof value === "boolean") return value;
    if (defaultValue !== undefined) return defaultValue;
    return false;
  }

  /**
   * Get array parameter with default
   */
  protected getArray<T>(
    params: Record<string, unknown>,
    key: string,
    defaultValue?: T[],
  ): T[] {
    const value = params[key];
    if (Array.isArray(value)) return value as T[];
    if (defaultValue !== undefined) return defaultValue;
    return [];
  }

  /**
   * Get object parameter with default
   */
  protected getObject<T extends Record<string, unknown>>(
    params: Record<string, unknown>,
    key: string,
    defaultValue?: T,
  ): T {
    const value = params[key];
    if (typeof value === "object" && value !== null && !Array.isArray(value)) {
      return value as T;
    }
    if (defaultValue !== undefined) return defaultValue;
    return {} as T;
  }
}

/**
 * Helper to create tool definitions
 */
export function defineTool(
  name: string,
  description: string,
  parameters: ToolDefinition["parameters"],
): ToolDefinition {
  return {
    name,
    description,
    parameters,
  };
}
