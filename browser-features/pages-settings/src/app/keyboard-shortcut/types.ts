import type { FirefoxKeyEntry } from "../../../../modules/common/NRKeySyncTypes.ts";
import type { KeyboardShortcutConfig, ShortcutConfig } from "../../types/pref.ts";

export interface ShortcutsSettingsProps {
  config: KeyboardShortcutConfig;
  addShortcut: (action: string, shortcut: ShortcutConfig) => Promise<boolean>;
  updateShortcut: (action: string, shortcut: ShortcutConfig) => Promise<boolean>;
  deleteShortcut: (action: string) => Promise<boolean>;
}

export interface ShortcutEditorProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (shortcut: ShortcutConfig) => Promise<boolean>;
  initialShortcut: ShortcutConfig | null;
  existingShortcuts: ShortcutConfig[];
  actionId: string;
  /**
   * Firefox live key entries used for conflict warnings.
   * When omitted, the component loads them via useFirefoxKeys().
   * Primarily for testing — pass a fixed list to avoid RPC dependencies.
   */
  firefoxKeys?: FirefoxKeyEntry[];
}

export interface FirefoxKeyEditorProps {
  entry: FirefoxKeyEntry;
  onClose: () => void;
}
