// SPDX-License-Identifier: MPL-2.0
import { rpc } from "../../lib/rpc/rpc.ts";
import type { DesignPreferenceTransport } from "./types.ts";
const PREF = "floorp.design.configs";
function object(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Invalid design configuration");
  }
  return value as Record<string, unknown>;
}
export function parseStartDisabled(raw: string): boolean {
  const config = object(JSON.parse(raw));
  return object(config.uiCustomization).disableFloorpStart === true;
}
export function patchStartDisabled(raw: string, disabled: boolean): string {
  const config = object(JSON.parse(raw));
  return JSON.stringify({
    ...config,
    uiCustomization: {
      ...object(config.uiCustomization),
      disableFloorpStart: disabled,
    },
  });
}
export async function getStartDisabled(
  transport: DesignPreferenceTransport = rpc,
): Promise<boolean> {
  const state = await transport.getStringPrefState(PREF);
  if (state.typeMismatch || state.value === null) {
    throw new Error("Design configuration is unavailable");
  }
  return parseStartDisabled(state.value);
}
export async function setStartDisabled(
  disabled: boolean,
  transport: DesignPreferenceTransport = rpc,
): Promise<void> {
  for (let attempt = 0; attempt < 4; attempt++) {
    const state = await transport.getStringPrefState(PREF);
    if (state.typeMismatch || state.value === null) {
      throw new Error("Design configuration is unavailable");
    }
    const result = await transport.compareAndSetStringPref(
      PREF,
      state.value,
      patchStartDisabled(state.value, disabled),
    );
    if (result.typeMismatch) {
      throw new Error("Design configuration type changed");
    }
    if (result.updated) return;
  }
  throw new Error("Design configuration changed concurrently");
}
