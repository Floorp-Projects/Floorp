import { rpc } from "../../lib/rpc/rpc.ts";
import type { CommandPaletteFormData } from "../../types/pref.ts";

const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";
const COMMAND_PALETTE_WIDTH_PREF = "floorp.commandPalette.width";
const COMMAND_PALETTE_MAX_HEIGHT_PREF = "floorp.commandPalette.maxHeight";
const COMMAND_PALETTE_OFFSET_TOP_PREF = "floorp.commandPalette.offsetTop";
const COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF = "floorp.commandPalette.horizontalAlign";

const DEFAULT_WIDTH = 560;
const DEFAULT_MAX_HEIGHT = 400;
const DEFAULT_OFFSET_TOP = 20;
const DEFAULT_HORIZONTAL_ALIGN = "center";

const WIDTH_BOUNDS = { min: 400, max: 1000 } as const;
const MAX_HEIGHT_BOUNDS = { min: 300, max: 800 } as const;
const OFFSET_TOP_BOUNDS = { min: 0, max: 60 } as const;
const VALID_HORIZONTAL_ALIGNS = ["center", "left", "right"] as const;

function clampInt(
  value: number,
  bounds: { min: number; max: number },
  fallback: number,
): number {
  if (!Number.isFinite(value)) return fallback;
  const i = Math.round(value);
  return Math.min(bounds.max, Math.max(bounds.min, i));
}

export async function saveCommandPaletteSettings(
  settings: Partial<CommandPaletteFormData>,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  try {
    await rpc.setBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      Boolean(settings.enabled),
    );

    if (settings.width !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_WIDTH_PREF,
        clampInt(Number(settings.width), WIDTH_BOUNDS, DEFAULT_WIDTH),
      );
    }

    if (settings.maxHeight !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_MAX_HEIGHT_PREF,
        clampInt(Number(settings.maxHeight), MAX_HEIGHT_BOUNDS, DEFAULT_MAX_HEIGHT),
      );
    }

    if (settings.offsetTop !== undefined) {
      await rpc.setIntPref(
        COMMAND_PALETTE_OFFSET_TOP_PREF,
        clampInt(Number(settings.offsetTop), OFFSET_TOP_BOUNDS, DEFAULT_OFFSET_TOP),
      );
    }

    if (settings.horizontalAlign !== undefined) {
      const v = String(settings.horizontalAlign);
      await rpc.setStringPref(
        COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
        (VALID_HORIZONTAL_ALIGNS as readonly string[]).includes(v)
          ? v
          : DEFAULT_HORIZONTAL_ALIGN,
      );
    }
  } catch (error) {
    console.error("[command-palette] Failed to save settings:", error);
  }
}

export async function getCommandPaletteSettings(): Promise<CommandPaletteFormData | null> {
  try {
    const enabled = await rpc.getBoolPref(COMMAND_PALETTE_ENABLED_PREF);
    const width = await rpc.getIntPref(COMMAND_PALETTE_WIDTH_PREF);
    const maxHeight = await rpc.getIntPref(COMMAND_PALETTE_MAX_HEIGHT_PREF);
    const offsetTop = await rpc.getIntPref(COMMAND_PALETTE_OFFSET_TOP_PREF);
    const horizontalAlign = await rpc.getStringPref(
      COMMAND_PALETTE_HORIZONTAL_ALIGN_PREF,
    );

    return {
      enabled: enabled === null ? true : enabled,
      width: clampInt(width ?? DEFAULT_WIDTH, WIDTH_BOUNDS, DEFAULT_WIDTH),
      maxHeight: clampInt(maxHeight ?? DEFAULT_MAX_HEIGHT, MAX_HEIGHT_BOUNDS, DEFAULT_MAX_HEIGHT),
      offsetTop: clampInt(offsetTop ?? DEFAULT_OFFSET_TOP, OFFSET_TOP_BOUNDS, DEFAULT_OFFSET_TOP),
      horizontalAlign: (VALID_HORIZONTAL_ALIGNS as readonly string[]).includes(
        horizontalAlign ?? DEFAULT_HORIZONTAL_ALIGN,
      )
        ? (horizontalAlign as string)
        : DEFAULT_HORIZONTAL_ALIGN,
    };
  } catch (error) {
    console.error("[command-palette] Failed to load settings:", error);
    return null;
  }
}
