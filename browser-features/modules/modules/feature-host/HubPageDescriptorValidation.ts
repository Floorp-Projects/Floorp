// SPDX-License-Identifier: MPL-2.0

import type { HubPageDescriptor } from "./HubRegistryTypes.ts";

export const HUB_DESCRIPTOR_MAX_BYTES = 2_048;

const forbiddenKeys = new Set(["__proto__", "prototype", "constructor"]);

export class HubDescriptorValidationError extends Error {
  constructor(readonly code: string) {
    super(code);
    this.name = "HubDescriptorValidationError";
  }
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function hasExactKeys(
  value: Record<string, unknown>,
  required: readonly string[],
  optional: readonly string[] = [],
): boolean {
  const keys = Object.keys(value);
  return keys.every((key) =>
    !forbiddenKeys.has(key) &&
    (required.includes(key) || optional.includes(key))
  ) && required.every((key) => Object.hasOwn(value, key));
}

export function isHubIdentifier(value: unknown): value is string {
  return typeof value === "string" && value.length <= 48 &&
    /^[a-z][a-z0-9-]*$/.test(value);
}

export function isHubLocalizationKey(value: unknown): value is string {
  return typeof value === "string" && value.length <= 128 &&
    /^[A-Za-z0-9][A-Za-z0-9_.:-]*$/.test(value);
}

function validateParams(value: unknown): HubPageDescriptor["view"]["params"] {
  if (value === undefined) return undefined;
  if (!isObject(value) || Object.keys(value).length > 8) {
    throw new HubDescriptorValidationError("invalid hub params");
  }
  const params: Record<string, string | number | boolean> = {};
  for (const [key, item] of Object.entries(value)) {
    if (forbiddenKeys.has(key) || !/^[A-Za-z][A-Za-z0-9_]{0,31}$/.test(key)) {
      throw new HubDescriptorValidationError("invalid hub param key");
    }
    if (typeof item === "string" && item.length <= 128) params[key] = item;
    else if (typeof item === "boolean") params[key] = item;
    else if (
      typeof item === "number" && Number.isFinite(item) &&
      Math.abs(item) <= 1_000_000_000
    ) params[key] = item;
    else throw new HubDescriptorValidationError("invalid hub param value");
  }
  return Object.freeze(params);
}

export function validateHubPageDescriptor(
  value: unknown,
  featureId: unknown,
): HubPageDescriptor {
  if (!isHubIdentifier(featureId)) {
    throw new HubDescriptorValidationError("invalid feature ID");
  }
  if (
    !isObject(value) ||
    !hasExactKeys(value, ["localId", "route", "titleKey", "view"], [
      "navigation",
    ])
  ) throw new HubDescriptorValidationError("invalid hub descriptor schema");
  const localId = value.localId;
  if (!isHubIdentifier(localId)) {
    throw new HubDescriptorValidationError("invalid hub local ID");
  }
  const route = value.route;
  if (route !== `/extensions/${featureId}/${localId}`) {
    throw new HubDescriptorValidationError("non-canonical hub route");
  }
  if (!isHubLocalizationKey(value.titleKey)) {
    throw new HubDescriptorValidationError("invalid hub title key");
  }
  let navigation: HubPageDescriptor["navigation"];
  if (value.navigation !== undefined) {
    if (
      !isObject(value.navigation) ||
      !hasExactKeys(value.navigation, ["labelKey"], ["order"]) ||
      !isHubLocalizationKey(value.navigation.labelKey) ||
      (value.navigation.order !== undefined &&
        (!Number.isSafeInteger(value.navigation.order) ||
          (value.navigation.order as number) < -1_000 ||
          (value.navigation.order as number) > 1_000))
    ) throw new HubDescriptorValidationError("invalid hub navigation");
    navigation = {
      labelKey: value.navigation.labelKey,
      ...(value.navigation.order === undefined
        ? {}
        : { order: value.navigation.order as number }),
    };
  }
  const view = value.view;
  if (
    !isObject(view) ||
    !hasExactKeys(view, ["kind", "titleKey", "bodyKey"], ["params"]) ||
    view.kind !== "message" || !isHubLocalizationKey(view.titleKey) ||
    !isHubLocalizationKey(view.bodyKey)
  ) throw new HubDescriptorValidationError("invalid hub message view");
  const descriptor: HubPageDescriptor = {
    localId,
    route,
    titleKey: value.titleKey,
    ...(navigation ? { navigation } : {}),
    view: {
      kind: "message",
      titleKey: view.titleKey,
      bodyKey: view.bodyKey,
      ...(view.params === undefined
        ? {}
        : { params: validateParams(view.params) }),
    },
  };
  return JSON.parse(JSON.stringify(descriptor)) as HubPageDescriptor;
}

export function parseHubPageDescriptor(
  json: string,
  featureId: string,
): HubPageDescriptor {
  if (new TextEncoder().encode(json).byteLength > HUB_DESCRIPTOR_MAX_BYTES) {
    throw new HubDescriptorValidationError("hub descriptor too large");
  }
  let value: unknown;
  try {
    value = JSON.parse(json);
  } catch {
    throw new HubDescriptorValidationError("invalid hub descriptor JSON");
  }
  return validateHubPageDescriptor(value, featureId);
}
