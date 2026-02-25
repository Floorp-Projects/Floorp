/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  FormProvider,
  useForm,
  useFormContext,
  useWatch,
} from "react-hook-form";
import { useTranslation } from "react-i18next";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Input } from "@/components/common/input.tsx";
import { Switch } from "@/components/common/switch.tsx";
import {
  getLLMProvidersSettings,
  saveLLMProvidersSettings,
} from "./dataManager.ts";
import type { LLMProvidersFormData, LLMProviderType } from "@/types/pref.ts";
import { Cpu, Globe, Key, Sparkles } from "lucide-react";
import { FLOORP_LLM_MODEL_OPTIONS } from "../../../../modules/common/defines.ts";

// Shared model lists (same source as ProviderRegistry defaults)
const PROVIDER_MODELS: Record<LLMProviderType, string[]> = {
  openai: [...FLOORP_LLM_MODEL_OPTIONS.openai],
  anthropic: [...FLOORP_LLM_MODEL_OPTIONS.anthropic],
  ollama: [...FLOORP_LLM_MODEL_OPTIONS.ollama],
  "openai-compatible": [],
  "anthropic-compatible": [],
};

const PROVIDER_INFO: Record<
  LLMProviderType,
  { name: string; description: string; requiresApiKey: boolean }
> = {
  openai: {
    name: "OpenAI",
    description: "GPT-5 and reasoning models via OpenAI API",
    requiresApiKey: true,
  },
  anthropic: {
    name: "Anthropic",
    description: "Claude 4 models for advanced reasoning",
    requiresApiKey: true,
  },
  ollama: {
    name: "Ollama",
    description: "Run Llama, DeepSeek, Qwen locally",
    requiresApiKey: false,
  },
  "openai-compatible": {
    name: "OpenAI Compatible",
    description:
      "Any provider with OpenAI-compatible API (LM Studio, vLLM, etc.)",
    requiresApiKey: true,
  },
  "anthropic-compatible": {
    name: "Anthropic Compatible",
    description: "Any provider with Anthropic-compatible API",
    requiresApiKey: true,
  },
};

export default function LLMProvidersPage() {
  const { t } = useTranslation();
  const methods = useForm<LLMProvidersFormData>({ defaultValues: {} });
  const { control, setValue, reset } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getLLMProvidersSettings();
      if (!values) return;
      reset(values);
    };

    fetchDefaultValues();
    document.documentElement.addEventListener("focus", fetchDefaultValues);

    return () => {
      document.documentElement.removeEventListener("focus", fetchDefaultValues);
    };
  }, [reset]);

  React.useEffect(() => {
    if (Object.keys(watchAll).length === 0 || !watchAll.providers) return;
    saveLLMProvidersSettings(watchAll as LLMProvidersFormData);
  }, [watchAll]);

  const providerTypes: LLMProviderType[] = [
    "openai",
    "anthropic",
    "ollama",
    "openai-compatible",
    "anthropic-compatible",
  ];

  return (
    <FormProvider {...methods}>
      <div className="space-y-6 p-6">
        <div>
          <h1 className="text-2xl font-bold">{t("llmProviders.title")}</h1>
          <p className="text-muted-foreground mt-2">
            {t("llmProviders.description")}
          </p>
        </div>

        {/* Default Provider Selection */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Sparkles className="w-5 h-5" />
              {t("llmProviders.defaultProvider")}
            </CardTitle>
            <CardDescription>
              {t("llmProviders.defaultProviderDescription")}
            </CardDescription>
          </CardHeader>
          <CardContent>
            <select
              className="w-full p-2 border rounded-md bg-background"
              value={(watchAll.defaultProvider as string) || ""}
              onChange={(e) =>
                setValue("defaultProvider", e.target.value as LLMProviderType)}
            >
              <option value="">{t("llmProviders.selectProvider")}</option>
              {providerTypes.map((type) => (
                <option key={type} value={type}>
                  {PROVIDER_INFO[type].name}
                </option>
              ))}
            </select>
          </CardContent>
        </Card>

        {/* Provider Cards */}
        <div className="grid gap-6">
          {providerTypes.map((providerType) => (
            <ProviderCard
              key={providerType}
              providerType={providerType}
              info={PROVIDER_INFO[providerType]}
              models={PROVIDER_MODELS[providerType]}
            />
          ))}
        </div>
      </div>
    </FormProvider>
  );
}

interface ProviderCardProps {
  providerType: LLMProviderType;
  info: { name: string; description: string; requiresApiKey: boolean };
  models: string[];
}

function ProviderCard({ providerType, info, models }: ProviderCardProps) {
  const { t } = useTranslation();
  const { control, setValue } = useFormContext<LLMProvidersFormData>();
  const watchAll = useWatch({ control });

  const provider = watchAll.providers?.[providerType];

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <div>
            <CardTitle>{info.name}</CardTitle>
            <CardDescription>{info.description}</CardDescription>
          </div>
          <Switch
            checked={provider?.enabled ?? false}
            onChange={(e: React.ChangeEvent<HTMLInputElement>) =>
              setValue(`providers.${providerType}.enabled`, e.target.checked)}
          />
        </div>
      </CardHeader>
      <CardContent className="space-y-4">
        {/* API Key (for providers that require it) */}
        {info.requiresApiKey && (
          <div className="space-y-2">
            <label className="text-sm font-medium flex items-center gap-2">
              <Key className="w-4 h-4" />
              {t("llmProviders.apiKey")}
            </label>
            <Input
              type="password"
              placeholder={t("llmProviders.apiKeyPlaceholder")}
              value={(provider?.apiKey as string) || ""}
              onChange={(e) =>
                setValue(
                  `providers.${providerType}.apiKey`,
                  e.target.value || null,
                )}
            />
          </div>
        )}

        {/* Base URL */}
        <div className="space-y-2">
          <label className="text-sm font-medium flex items-center gap-2">
            <Globe className="w-4 h-4" />
            {t("llmProviders.baseUrl")}
          </label>
          <Input
            placeholder={providerType === "ollama"
              ? "http://localhost:11434"
              : t("llmProviders.baseUrlPlaceholder")}
            value={(provider?.baseUrl as string) || ""}
            onChange={(e) =>
              setValue(
                `providers.${providerType}.baseUrl`,
                e.target.value || null,
              )}
          />
        </div>

        {/* Default Model */}
        <div className="space-y-2">
          <label className="text-sm font-medium flex items-center gap-2">
            <Cpu className="w-4 h-4" />
            {t("llmProviders.defaultModel")}
          </label>
          {models.length > 0
            ? (
              <select
                className="w-full p-2 border rounded-md bg-background"
                value={(provider?.defaultModel as string) || ""}
                onChange={(e) =>
                  setValue(
                    `providers.${providerType}.defaultModel`,
                    e.target.value,
                  )}
              >
                <option value="">{t("llmProviders.selectModel")}</option>
                {models.map((model) => (
                  <option key={model} value={model}>
                    {model}
                  </option>
                ))}
              </select>
            )
            : (
              <Input
                placeholder={t("llmProviders.defaultModel")}
                value={(provider?.defaultModel as string) || ""}
                onChange={(e) =>
                  setValue(
                    `providers.${providerType}.defaultModel`,
                    e.target.value,
                  )}
              />
            )}
        </div>
      </CardContent>
    </Card>
  );
}
