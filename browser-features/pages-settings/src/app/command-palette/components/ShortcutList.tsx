import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { Plus, X } from "lucide-react";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Input } from "@/components/common/input.tsx";
import {
  loadSelectableCommands,
  loadShortcuts,
  saveShortcuts,
  isReservedShortcutPrefix,
  type CommandPaletteShortcut,
  type SelectableCommand,
} from "../dataManager.ts";

/**
 * Groups the selectable command catalog by `category`, preserving first-seen
 * order so the `<optgroup>` layout stays stable across renders. Commands with
 * a missing/empty category are bucketed under "".
 */
function groupByCategory(
  commands: SelectableCommand[],
): Map<string, SelectableCommand[]> {
  const grouped = new Map<string, SelectableCommand[]>();
  for (const command of commands) {
    const bucket = grouped.get(command.category);
    if (bucket) {
      bucket.push(command);
    } else {
      grouped.set(command.category, [command]);
    }
  }
  return grouped;
}

export function ShortcutList() {
  const { t } = useTranslation();
  const [shortcuts, setShortcuts] = useState<CommandPaletteShortcut[]>([]);
  const [selectable, setSelectable] = useState<SelectableCommand[]>([]);
  const [newPrefix, setNewPrefix] = useState("");
  const [newCommandId, setNewCommandId] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  // Load shortcuts + selectable catalog in parallel on mount.
  useEffect(() => {
    let cancelled = false;
    const load = async (): Promise<void> => {
      try {
        const [loadedShortcuts, loadedSelectable] = await Promise.all([
          loadShortcuts(),
          loadSelectableCommands(),
        ]);
        if (cancelled) return;
        setShortcuts(loadedShortcuts);
        setSelectable(loadedSelectable);
      } catch (error) {
        console.error("[command-palette] Failed to load shortcuts:", error);
      } finally {
        if (!cancelled) setIsLoading(false);
      }
    };
    load();
    return () => {
      cancelled = true;
    };
  }, []);

  const grouped = useMemo(() => groupByCategory(selectable), [selectable]);

  /** Built-in shortcuts that cannot be changed or removed from this UI. */
  const reservedRows = [
    { prefix: "s", commandLabel: t("commandPalette.shortcuts.reservedWebSearch") },
    { prefix: "t", commandLabel: t("commandPalette.shortcuts.reservedTabSearch") },
    { prefix: "b", commandLabel: t("commandPalette.shortcuts.reservedBookmarkSearch") },
    { prefix: "h", commandLabel: t("commandPalette.shortcuts.reservedHistorySearch") },
  ];

  /** User-defined shortcuts, excluding reserved prefixes. */
  const userShortcuts = shortcuts.filter(
    (s) => !isReservedShortcutPrefix(s.prefix),
  );

  /** Resolves a command id to its human-readable label, falling back to the id. */
  const labelFor = (commandId: string): string => {
    const found = selectable.find((command) => command.id === commandId);
    return found ? found.label : commandId;
  };

  const persist = (updated: CommandPaletteShortcut[]): void => {
    setShortcuts(updated);
    // Best-effort save; dataManager already swallows errors.
    saveShortcuts(updated).catch((saveError) => {
      console.error("[command-palette] Failed to save shortcuts:", saveError);
    });
  };

  const handleAdd = (): void => {
    const prefix = newPrefix.trim();

    if (prefix.length === 0) {
      setError(t("commandPalette.shortcuts.errorEmpty"));
      return;
    }
    // `@` is the palette's input delimiter, never part of a stored prefix.
    if (prefix.includes("@")) {
      setError(t("commandPalette.shortcuts.errorAt"));
      return;
    }
    // Reserved check comes before the duplicate check: when a stale reserved
    // entry ("s"/"t") lingers in the pref (invisible in this UI), the reserved
    // error must surface instead of a misleading duplicate error.
    if (isReservedShortcutPrefix(prefix)) {
      setError(t("commandPalette.shortcuts.errorReserved"));
      return;
    }
    if (shortcuts.some((shortcut) => shortcut.prefix === prefix)) {
      setError(t("commandPalette.shortcuts.errorDuplicate"));
      return;
    }
    if (newCommandId.length === 0) {
      setError(t("commandPalette.shortcuts.errorCommandRequired"));
      return;
    }

    persist([...shortcuts, { prefix, commandId: newCommandId }]);
    setNewPrefix("");
    setNewCommandId("");
    setError(null);
  };

  const handleRemove = (prefix: string): void => {
    persist(shortcuts.filter((shortcut) => shortcut.prefix !== prefix));
  };

  const handlePrefixKeyDown = (e: React.KeyboardEvent<HTMLInputElement>): void => {
    if (e.key === "Enter") {
      e.preventDefault();
      handleAdd();
    }
  };

  const canAdd = selectable.length > 0;

  if (isLoading) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            {t("commandPalette.shortcuts.title")}
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="flex items-center gap-2 text-muted-foreground">
            <div className="size-4 rounded-full bg-muted animate-pulse" />
            <span className="text-sm">{t("common.loading")}</span>
          </div>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          {t("commandPalette.shortcuts.title")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.shortcuts.description")}
          <p className="mt-1">
            {t("commandPalette.shortcuts.reservedDescription")}
          </p>
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Reserved shortcuts section */}
        <div className="space-y-2">
          <h3 className="text-sm font-medium">
            {t("commandPalette.shortcuts.reservedTitle")}
          </h3>
          {reservedRows.map((row) => (
            <div
              key={row.prefix}
              className="flex items-center justify-between bg-muted/40 hover:bg-muted/60 px-3 py-2.5 rounded-md transition-colors"
            >
              <div className="flex items-center gap-2.5 min-w-0">
                <code className="text-sm font-mono shrink-0">
                  @{row.prefix}
                </code>
                <span className="text-muted-foreground shrink-0">→</span>
                <span className="text-sm truncate">{row.commandLabel}</span>
              </div>
              <span className="text-xs shrink-0 rounded bg-muted px-2 py-0.5 text-muted-foreground">
                {t("commandPalette.shortcuts.reservedBadge")}
              </span>
            </div>
          ))}
        </div>

        {/* Add form */}
        {canAdd
          ? (
            <div className="space-y-3">
              <div className="flex flex-col sm:flex-row gap-3">
                <div className="flex flex-col gap-1 sm:w-40">
                  <label
                    htmlFor="command-palette-shortcut-prefix"
                    className="text-sm font-medium"
                  >
                    {t("commandPalette.shortcuts.prefix")}
                  </label>
                  <Input
                    id="command-palette-shortcut-prefix"
                    type="text"
                    placeholder={t("commandPalette.shortcuts.prefixPlaceholder")}
                    value={newPrefix}
                    onChange={(e: React.ChangeEvent<HTMLInputElement>) =>
                      setNewPrefix(e.target.value)}
                    onKeyDown={handlePrefixKeyDown}
                    autoComplete="off"
                  />
                </div>
                <div className="flex flex-col gap-1 flex-1">
                  <label
                    htmlFor="command-palette-shortcut-command"
                    className="text-sm font-medium"
                  >
                    {t("commandPalette.shortcuts.command")}
                  </label>
                  <select
                    id="command-palette-shortcut-command"
                    value={newCommandId}
                    onChange={(e) => setNewCommandId(e.target.value)}
                    className="flex h-10 w-full rounded-md border border-base-content/30 bg-base-100 px-3 py-2 text-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-base-content/20 disabled:cursor-not-allowed disabled:opacity-50"
                  >
                    <option value="">
                      {t("commandPalette.shortcuts.commandPlaceholder")}
                    </option>
                    {[...grouped.entries()].map(([category, commands]) => (
                      <optgroup
                        key={category}
                        label={
                          category.length > 0
                            ? t(
                              `commandPalette.categoryNames.${category}`,
                              { defaultValue: category },
                            )
                            : t("commandPalette.shortcuts.uncategorized")
                        }
                      >
                        {commands.map((command) => (
                          <option key={command.id} value={command.id}>
                            {command.label}
                          </option>
                        ))}
                      </optgroup>
                    ))}
                  </select>
                </div>
                <div className="flex items-end">
                  <button
                    type="button"
                    onClick={handleAdd}
                    className="h-10 px-4 bg-primary text-primary-foreground rounded-md hover:bg-primary/90 disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2 transition-colors whitespace-nowrap"
                  >
                    <Plus className="size-4" />
                    {t("commandPalette.shortcuts.add")}
                  </button>
                </div>
              </div>
              {error && (
                <p className="text-sm text-destructive">{error}</p>
              )}
            </div>
          )
          : (
            <div className="flex items-center gap-3 py-4 px-3 border border-dashed border-muted-foreground/30 rounded-md">
              <p className="text-sm text-muted-foreground">
                {t("commandPalette.shortcuts.noCommands")}
              </p>
            </div>
          )}

        {/* Registered shortcuts list */}
        {userShortcuts.length > 0
          ? (
            <div className="space-y-2">
              {userShortcuts.map((shortcut) => (
                <div
                  key={shortcut.prefix}
                  className="group flex items-center justify-between bg-muted/40 hover:bg-muted/60 px-3 py-2.5 rounded-md transition-colors"
                >
                  <div className="flex items-center gap-2.5 min-w-0">
                    <code className="text-sm font-mono shrink-0">
                      @{shortcut.prefix}
                    </code>
                    <span className="text-muted-foreground shrink-0">→</span>
                    <span className="text-sm truncate">
                      {labelFor(shortcut.commandId)}
                    </span>
                  </div>
                  <button
                    type="button"
                    onClick={() => handleRemove(shortcut.prefix)}
                    className="opacity-60 hover:opacity-100 text-destructive p-1 rounded hover:bg-destructive/10 transition-all shrink-0"
                    aria-label={t("commandPalette.shortcuts.remove")}
                  >
                    <X className="size-4" />
                  </button>
                </div>
              ))}
            </div>
          )
          : (
            <div className="flex items-center gap-3 py-4 px-3 border border-dashed border-muted-foreground/30 rounded-md">
              <p className="text-sm text-muted-foreground">
                {t("commandPalette.shortcuts.empty")}
              </p>
            </div>
          )}
      </CardContent>
    </Card>
  );
}

export default ShortcutList;
