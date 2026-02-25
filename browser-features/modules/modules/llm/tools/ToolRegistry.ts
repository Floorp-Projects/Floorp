/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tool Registry for managing available tools
 */

import type { ToolDefinition, ToolResult } from "../types.ts";
import type { Tool, ToolContext } from "./Tool.ts";
import { WebScraperTool } from "./WebScraperTool.ts";

/**
 * Registry for all available tools
 */
export class ToolRegistry {
  private tools: Map<string, Tool> = new Map();
  private webscraperTool: WebScraperTool | null = null;

  constructor() {
    // Register built-in tools
    this.registerBuiltInTools();
  }

  /**
   * Register built-in tools
   */
  private registerBuiltInTools(): void {
    this.webscraperTool = new WebScraperTool();
    this.register(this.webscraperTool);
  }

  /**
   * Register a tool
   */
  register(tool: Tool): void {
    this.tools.set(tool.definition.name, tool);
  }

  /**
   * Unregister a tool by name
   */
  unregister(name: string): boolean {
    return this.tools.delete(name);
  }

  /**
   * Get a tool by name
   */
  get(name: string): Tool | undefined {
    return this.tools.get(name);
  }

  /**
   * Check if a tool exists
   */
  has(name: string): boolean {
    return this.tools.has(name);
  }

  /**
   * Get all registered tools
   */
  getAll(): Tool[] {
    return Array.from(this.tools.values());
  }

  /**
   * Get all tool definitions for LLM
   */
  getDefinitions(): ToolDefinition[] {
    return this.getAll().map((tool) => tool.definition);
  }

  /**
   * Get tool definitions for specific tools (by name)
   */
  getDefinitionsFor(names: string[]): ToolDefinition[] {
    return names
      .map((name) => this.tools.get(name))
      .filter((tool): tool is Tool => tool !== undefined)
      .map((tool) => tool.definition);
  }

  /**
   * Execute a tool by name
   */
  async execute(
    name: string,
    params: Record<string, unknown>,
    context?: ToolContext,
    toolCallId?: string,
  ): Promise<ToolResult> {
    const tool = this.tools.get(name);
    if (!tool) {
      return {
        toolCallId: toolCallId ?? "unknown",
        result: `Tool '${name}' not found`,
        isError: true,
      };
    }

    try {
      const result = await tool.execute(params, context);
      if (toolCallId && !result.toolCallId) {
        return { ...result, toolCallId };
      }
      return result;
    } catch (e) {
      const error = e as Error;
      return {
        toolCallId: toolCallId ?? "unknown",
        result: `Tool execution error: ${error.message}`,
        isError: true,
      };
    }
  }

  /**
   * Set the WebScraper actor reference
   */
  setWebScraperActor(
    actor: import("./WebScraperTool.ts").WebScraperActorInterface,
  ): void {
    if (this.webscraperTool) {
      this.webscraperTool.setActorRef(actor);
    }
  }

  /**
   * Create a tool registry with specific tools enabled
   */
  static createWithTools(toolNames: string[]): ToolRegistry {
    const registry = new ToolRegistry();
    // Filter to only specified tools
    const allTools = registry.getAll();
    registry.tools.clear();
    for (const tool of allTools) {
      if (toolNames.includes(tool.definition.name)) {
        registry.register(tool);
      }
    }
    return registry;
  }
}

/**
 * Interface for WebScraper actor (re-exported for convenience)
 */
export type { WebScraperActorInterface } from "./WebScraperTool.ts";

/**
 * Export tool classes for external use
 */
export { WebScraperTool } from "./WebScraperTool.ts";
export { BaseTool, defineTool } from "./Tool.ts";
export type { Tool, ToolContext } from "./Tool.ts";
