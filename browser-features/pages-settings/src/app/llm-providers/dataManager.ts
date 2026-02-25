/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { rpc } from "@/lib/rpc/rpc.ts";
import type { LLMProvidersFormData, LLMProviderType } from "@/types/pref.ts";
import { FLOORP_LLM_DEFAULT_MODELS } from "../../../../modules/common/defines.ts";

const PREF_KEY = "floorp.llm.providers";

export async function saveLLMProvidersSettings(
  data: LLMProvidersFormData,
): Promise<void> {
  await rpc.setStringPref(PREF_KEY, JSON.stringify(data));
}

export async function getLLMProvidersSettings(): Promise<LLMProvidersFormData | null> {
  const result = await rpc.getStringPref(PREF_KEY);
  if (!result) {
    return getDefaultSettings();
  }

  try {
    return JSON.parse(result) as LLMProvidersFormData;
  } catch {
    return getDefaultSettings();
  }
}

export function getDefaultSettings(): LLMProvidersFormData {
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

export async function setDefaultProvider(
  provider: LLMProviderType,
): Promise<void> {
  const settings = await getLLMProvidersSettings();
  if (!settings) return;

  settings.defaultProvider = provider;
  await saveLLMProvidersSettings(settings);
}
