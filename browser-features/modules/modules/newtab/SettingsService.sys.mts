// SPDX-License-Identifier: MPL-2.0
import {
  mergeNewTabSettings,
  NEWTAB_SETTINGS_PREF,
  NewTabSettingsError,
  parseNewTabSettings,
} from "#libs/newtab/settings.ts";
import type {
  NewTabSettings,
  NewTabSettingsPatch,
} from "#libs/newtab/types.ts";

interface PreferenceAdapter {
  read(): string | null;
  write(raw: string): void;
  observe(listener: () => void): () => void;
}

/** A single parent-process writer owns the full preference, including legacy keys. */
export class NewTabSettingsStore {
  private queue: Promise<void> = Promise.resolve();
  private lastRaw: string | null | undefined;
  private revision = 0;
  private listeners = new Set<(settings: NewTabSettings) => void>();
  private writing = false;
  private unobserve: (() => void) | null = null;

  constructor(private readonly adapter: PreferenceAdapter) {}

  get(): NewTabSettings {
    const raw = this.adapter.read();
    const settings = parseNewTabSettings(raw);
    if (this.lastRaw === undefined) this.revision = settings.revision;
    else if (raw !== this.lastRaw) {
      this.revision = Math.max(this.revision + 1, settings.revision);
    }
    this.lastRaw = raw;
    settings.revision = this.revision;
    return settings;
  }

  patch(
    expectedRevision: number,
    changes: NewTabSettingsPatch,
  ): Promise<NewTabSettings> {
    return this.enqueue(() => {
      const current = this.get();
      if (
        !Number.isSafeInteger(expectedRevision) ||
        current.revision !== expectedRevision
      ) {
        throw new NewTabSettingsError(
          "CONFLICT",
          "The new tab settings changed in another window. Reload them and try again.",
        );
      }
      return this.save(mergeNewTabSettings(current, changes));
    });
  }

  updateInternal(changes: NewTabSettingsPatch): Promise<NewTabSettings> {
    return this.enqueue(() =>
      this.save(mergeNewTabSettings(this.get(), changes, true))
    );
  }

  subscribe(listener: (settings: NewTabSettings) => void): () => void {
    this.get();
    this.listeners.add(listener);
    if (!this.unobserve) {
      this.unobserve = this.adapter.observe(() => {
        if (this.writing) return;
        try {
          this.notify(this.get());
        } catch {
          /* Invalid external writes must not be repaired or overwritten here. */
        }
      });
    }
    return () => {
      this.listeners.delete(listener);
      if (this.listeners.size === 0) {
        this.unobserve?.();
        this.unobserve = null;
      }
    };
  }

  dispose(): void {
    this.listeners.clear();
    this.unobserve?.();
    this.unobserve = null;
  }

  private enqueue(work: () => NewTabSettings): Promise<NewTabSettings> {
    const next = this.queue.then(work);
    this.queue = next.then(() => {}, () => {});
    return next;
  }

  private save(settings: NewTabSettings): NewTabSettings {
    const raw = JSON.stringify(settings);
    // get/merge/write is deliberately synchronous inside the serialized job.
    // Gecko prefs and any legacy observer cannot interleave an awaited write.
    this.writing = true;
    try {
      this.adapter.write(raw);
    } finally {
      this.writing = false;
    }
    if (this.adapter.read() !== raw) {
      this.get();
      throw new NewTabSettingsError(
        "CONFLICT",
        "The new tab settings changed while saving. Reload them and try again.",
      );
    }
    this.lastRaw = raw;
    this.revision = settings.revision;
    this.notify(settings);
    return structuredClone(settings);
  }

  private notify(settings: NewTabSettings): void {
    for (const listener of this.listeners) {
      try {
        listener(structuredClone(settings));
      } catch (error) {
        console.error("[NewTabSettings] Settings listener failed", error);
      }
    }
  }
}

function assertParentProcess(): void {
  if (Services.appinfo.processType !== Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    throw new NewTabSettingsError(
      "READ_ONLY",
      "New tab settings can only be accessed by the browser parent.",
    );
  }
}

export const SettingsService = new NewTabSettingsStore({
  read() {
    assertParentProcess();
    const type = Services.prefs.getPrefType(NEWTAB_SETTINGS_PREF);
    if (type === Services.prefs.PREF_INVALID) return null;
    if (type !== Services.prefs.PREF_STRING) {
      throw new NewTabSettingsError(
        "VALIDATION",
        "The saved new tab preference has the wrong type.",
      );
    }
    return Services.prefs.getStringPref(NEWTAB_SETTINGS_PREF);
  },
  write(raw) {
    assertParentProcess();
    if (Services.prefs.prefIsLocked(NEWTAB_SETTINGS_PREF)) {
      throw new NewTabSettingsError(
        "READ_ONLY",
        "New tab settings are managed by your organization.",
      );
    }
    Services.prefs.setStringPref(NEWTAB_SETTINGS_PREF, raw);
  },
  observe(listener) {
    assertParentProcess();
    Services.prefs.addObserver(NEWTAB_SETTINGS_PREF, listener);
    return () => Services.prefs.removeObserver(NEWTAB_SETTINGS_PREF, listener);
  },
});
