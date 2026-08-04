import { rpc } from "../../lib/rpc/rpc.ts";
import type { CommandPaletteFormData } from "../../types/pref.ts";

const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";

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
  } catch (error) {
    console.error("[command-palette] Failed to save settings:", error);
  }
}

export async function getCommandPaletteSettings(): Promise<CommandPaletteFormData | null> {
  try {
    const enabled = await rpc.getBoolPref(COMMAND_PALETTE_ENABLED_PREF);

    if (enabled === null) {
      return null;
    }

    return { enabled };
  } catch (error) {
    console.error("[command-palette] Failed to load settings:", error);
    return null;
  }
}
