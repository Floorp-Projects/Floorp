/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provider Registry for centralized management of LLM providers
 */

import {
  FLOORP_LLM_DEFAULT_MODELS,
  FLOORP_LLM_MODEL_OPTIONS,
} from "../../../common/defines.ts";
import type { LLMProvider } from "./Provider.ts";
import type { ProviderType, ProviderConfig } from "../types.ts";
import { OpenAIProvider } from "./OpenAIProvider.ts";
import { AnthropicProvider } from "./AnthropicProvider.ts";
import { OllamaProvider } from "./OllamaProvider.ts";

/**
 * Provider metadata for display and configuration
 */
export interface ProviderMetadata {
  /** Provider type identifier */
  type: ProviderType;
  /** Display name for UI */
  displayName: string;
  /** Description of the provider */
  description: string;
  /** Default base URL (if applicable) */
  defaultBaseUrl?: string;
  /** Default model for this provider */
  defaultModel: string;
  /** Available models (static list, may be extended by API) */
  availableModels: string[];
  /** Whether API key is required */
  requiresApiKey: boolean;
  /** Whether base URL is configurable */
  configurableBaseUrl: boolean;
  /** Icon or logo identifier */
  icon?: string;
  /** Provider website URL */
  websiteUrl?: string;
}

/**
 * Predefined provider metadata
 */
export const PROVIDER_METADATA: Record<ProviderType, ProviderMetadata> = {
  openai: {
    type: "openai",
    displayName: "OpenAI",
    description: "GPT-5, GPT-4, and other OpenAI models",
    defaultBaseUrl: "https://api.openai.com/v1",
    defaultModel: FLOORP_LLM_DEFAULT_MODELS.openai,
    availableModels: [...FLOORP_LLM_MODEL_OPTIONS.openai],
    requiresApiKey: true,
    configurableBaseUrl: true,
    icon: "openai",
    websiteUrl: "https://platform.openai.com",
  },
  anthropic: {
    type: "anthropic",
    displayName: "Anthropic",
    description: "Claude 4 and other Anthropic models",
    defaultBaseUrl: "https://api.anthropic.com/v1",
    defaultModel: FLOORP_LLM_DEFAULT_MODELS.anthropic,
    availableModels: [...FLOORP_LLM_MODEL_OPTIONS.anthropic],
    requiresApiKey: true,
    configurableBaseUrl: true,
    icon: "anthropic",
    websiteUrl: "https://www.anthropic.com",
  },
  ollama: {
    type: "ollama",
    displayName: "Ollama",
    description: "Local LLM inference with Ollama",
    defaultBaseUrl: "http://localhost:11434/api",
    defaultModel: FLOORP_LLM_DEFAULT_MODELS.ollama,
    availableModels: [...FLOORP_LLM_MODEL_OPTIONS.ollama],
    requiresApiKey: false,
    configurableBaseUrl: true,
    icon: "ollama",
    websiteUrl: "https://ollama.com",
  },
  custom: {
    type: "custom",
    displayName: "Custom Provider",
    description: "Custom OpenAI-compatible API endpoint",
    defaultBaseUrl: "",
    defaultModel: "",
    availableModels: [],
    requiresApiKey: false,
    configurableBaseUrl: true,
    icon: "custom",
  },
};

/**
 * Registry for managing LLM providers centrally
 */
export class ProviderRegistry {
  private providers: Map<ProviderType, LLMProvider> = new Map();
  private configs: Map<ProviderType, ProviderConfig> = new Map();

  /**
   * Get all available provider types
   */
  static getAvailableProviderTypes(): ProviderType[] {
    return Object.keys(PROVIDER_METADATA) as ProviderType[];
  }

  /**
   * Get metadata for a specific provider
   */
  static getProviderMetadata(type: ProviderType): ProviderMetadata {
    return PROVIDER_METADATA[type];
  }

  /**
   * Get all provider metadata
   */
  static getAllProviderMetadata(): ProviderMetadata[] {
    return Object.values(PROVIDER_METADATA);
  }

  /**
   * Register a provider instance
   */
  register(type: ProviderType, provider: LLMProvider): void {
    this.providers.set(type, provider);
  }

  /**
   * Unregister a provider
   */
  unregister(type: ProviderType): boolean {
    return this.providers.delete(type);
  }

  /**
   * Get a registered provider
   */
  get(type: ProviderType): LLMProvider | undefined {
    return this.providers.get(type);
  }

  /**
   * Check if a provider is registered
   */
  has(type: ProviderType): boolean {
    return this.providers.has(type);
  }

  /**
   * Get all registered providers
   */
  getAll(): Map<ProviderType, LLMProvider> {
    return new Map(this.providers);
  }

  /**
   * Set configuration for a provider type
   */
  setConfig(type: ProviderType, config: ProviderConfig): void {
    this.configs.set(type, config);
  }

  /**
   * Get configuration for a provider type
   */
  getConfig(type: ProviderType): ProviderConfig | undefined {
    return this.configs.get(type);
  }

  /**
   * Validate provider configuration
   */
  static validateConfig(config: ProviderConfig): boolean {
    const metadata = PROVIDER_METADATA[config.type];
    if (!metadata) {
      return false;
    }

    if (metadata.requiresApiKey && !config.apiKey) {
      return false;
    }

    return true;
  }

  /**
   * Create a provider instance from configuration
   */
  createProvider(config: ProviderConfig): LLMProvider {
    if (!ProviderRegistry.validateConfig(config)) {
      throw new Error(
        `Invalid configuration for provider type: ${config.type}`,
      );
    }

    this.setConfig(config.type, config);

    switch (config.type) {
      case "openai":
        return new OpenAIProvider(config);
      case "anthropic":
        return new AnthropicProvider(config);
      case "ollama":
        return new OllamaProvider(config);
      case "custom":
        return new OpenAIProvider(config);
      default: {
        const _exhaustiveCheck: never = config.type;
        throw new Error(`Unknown provider type: ${_exhaustiveCheck}`);
      }
    }
  }

  /**
   * Get or create a provider instance
   */
  getOrCreate(config: ProviderConfig): LLMProvider {
    const existing = this.get(config.type);
    if (existing) {
      const currentConfig = this.getConfig(config.type);
      if (JSON.stringify(currentConfig) === JSON.stringify(config)) {
        return existing;
      }
    }

    const provider = this.createProvider(config);
    this.register(config.type, provider);
    return provider;
  }

  /**
   * Clear all registered providers
   */
  clear(): void {
    this.providers.clear();
    this.configs.clear();
  }
}

// Singleton instance for global access
let globalRegistry: ProviderRegistry | null = null;

/**
 * Get the global provider registry instance
 */
export function getProviderRegistry(): ProviderRegistry {
  if (!globalRegistry) {
    globalRegistry = new ProviderRegistry();
  }
  return globalRegistry;
}

/**
 * Reset the global registry (mainly for testing)
 */
export function resetProviderRegistry(): void {
  globalRegistry = null;
}
