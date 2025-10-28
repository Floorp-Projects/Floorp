// SPDX-License-Identifier: MPL-2.0
// Experiments client (production-ready prototype)
// - Uses Services.prefs for profile-backed persistence
// - Deterministic assignment based on installId and experiment salt
// - Prefetches variant config from configUrl and caches in prefs
// - Exports: init(options), getVariant(experimentId), getConfig(experimentId)

// NOTE:
// This module intentionally relies only on Services (global) and
// prefs-based storage so it is safe to run in ESM chrome contexts.
// Keep comments and APIs in English for upstream readability.

// Convert procedural module into a small class so callers can create instances
// (useful for tests or future multiple-context scenarios).

const ASSIGNMENTS_PREF = "floorp.experiments.assignments.v1";
const INSTALLID_PREF = "floorp.experiments.installId";
// Optional pref to override the experiments manifest URL. Useful for local testing.
const MANIFEST_URL_PREF = "floorp.experiments.manifestUrl";
const CONFIG_CACHE_PREFIX = "floorp.experiments.config."; // + experimentId + ":" + variantId

// IMPORTANT: The experiments manifest URL is defined inside this module.
// Edit this constant when you want to change the source for experiments.
// This enforces that the fetch target is controlled by the shipping code
// rather than passed in at runtime.
const DEFAULT_EXPERIMENTS_URL =
  "https://updates.floorp.app/experiments/experiments.json";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

type Variant = {
  id: string;
  weight?: number;
  configUrl?: string;
  [k: string]: unknown;
};
type Experiment = {
  id: string;
  salt?: string;
  rollout?: number;
  start?: string;
  end?: string;
  variants?: Variant[];
  [k: string]: unknown;
};
type Assignment = {
  installId: string | null;
  variantId: string | null;
  assignedAt: string;
};
type ConfigStore = { fetchedAt: string; config: unknown };

export class ExperimentsClient {
  experimentsUrl: string | null = null;
  experiments: Experiment[] = [];
  assignments: Record<string, Assignment> = {};
  configs: Record<string, unknown> = {};
  installId: string | null = null;
  constructor() {
    this.experimentsUrl = DEFAULT_EXPERIMENTS_URL;
  }

  // Shared singleton instance. Use ExperimentsClient.getInstance() to access.
  static instance: ExperimentsClient | null = null;

  /**
   * Return the shared ExperimentsClient instance (create if needed).
   * Note: init() is still async and must be called by the consumer when they
   * want to fetch manifests/configs. getInstance() only guarantees a single
   * shared object is returned.
   */
  static getInstance(): ExperimentsClient {
    if (!ExperimentsClient.instance)
      ExperimentsClient.instance = new ExperimentsClient();
    return ExperimentsClient.instance;
  }

  private nowUtc(): Date {
    return new Date();
  }

  private parseDateFlexible(s: string | undefined | null): Date | null {
    if (!s) return null;
    const dateOnly = /^\d{4}-\d{2}-\d{2}$/;
    if (dateOnly.test(s)) {
      const [y, m, d] = (s as string).split("-").map((x) => parseInt(x, 10));
      return new Date(Date.UTC(y, m - 1, d));
    }
    const d = new Date(s as string);
    if (isNaN(d.getTime())) return null;
    return d;
  }

  private fnv1a32Hash(str: string): number {
    let h = 0x811c9dc5;
    for (let i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = (h >>> 0) * 0x01000193;
      h = h >>> 0;
    }
    return h >>> 0;
  }

  private percentFromHash(str: string): number {
    const h = this.fnv1a32Hash(str);
    return (h % 10000) / 100.0;
  }

  private getPrefString(key: string, fallback?: string | null): string | null {
    try {
      const val = Services.prefs.getStringPref(key);
      return typeof val === "string" ? val : (fallback ?? null);
    } catch {
      return fallback ?? null;
    }
  }

  private setPrefString(key: string, value: string): boolean {
    try {
      Services.prefs.setStringPref(key, value);
      return true;
    } catch {
      return false;
    }
  }

  private clearPref(key: string): boolean {
    try {
      if (Services.prefs.prefHasUserValue(key))
        Services.prefs.clearUserPref(key);
      return true;
    } catch {
      return false;
    }
  }

  private loadAssignmentsFromPrefs(): Record<string, Assignment> {
    try {
      const raw = this.getPrefString(ASSIGNMENTS_PREF, null);
      if (!raw) return {};
      return JSON.parse(raw) as Record<string, Assignment>;
    } catch (e) {
      console.error("Failed to parse experiments assignments: " + String(e));
      return {};
    }
  }

  private saveAssignmentsToPrefs(): void {
    try {
      this.setPrefString(ASSIGNMENTS_PREF, JSON.stringify(this.assignments));
    } catch (e) {
      console.error("Failed to save experiments assignments: " + String(e));
    }
  }

  private storageKeyForConfig(experimentId: string, variantId: string): string {
    return CONFIG_CACHE_PREFIX + experimentId + ":" + variantId;
  }

  private loadConfigFromPrefs(
    experimentId: string,
    variantId: string | null,
  ): unknown | null {
    try {
      if (!variantId) return null;
      const key = this.storageKeyForConfig(experimentId, variantId);
      const raw = this.getPrefString(key, null);
      if (!raw) return null;
      const parsed = JSON.parse(raw) as ConfigStore;
      return parsed.config || null;
    } catch (e) {
      console.error("Failed to load cached config: " + String(e));
      return null;
    }
  }

  private saveConfigToPrefs(
    experimentId: string,
    variantId: string | null,
    config: unknown,
  ): boolean {
    try {
      if (!variantId) return false;
      const key = this.storageKeyForConfig(experimentId, variantId);
      const store = { fetchedAt: new Date().toISOString(), config };
      this.setPrefString(key, JSON.stringify(store));
      return true;
    } catch (e) {
      console.error("Failed to save config to prefs: " + String(e));
      return false;
    }
  }

  private isExperimentActive(exp: Experiment): boolean {
    const now = this.nowUtc();
    const start = this.parseDateFlexible(exp.start);
    const end = this.parseDateFlexible(exp.end);
    if (start && now < start) return false;
    if (end && now >= end) return false;
    return true;
  }

  private chooseVariantForExperiment(
    exp: Experiment,
    installId: string,
  ): string | null {
    const salt = exp.salt || exp.id || "";
    const userPercent = this.percentFromHash(installId + "::" + salt);
    const rollout =
      typeof exp.rollout === "number"
        ? Math.max(0, Math.min(100, exp.rollout))
        : 100;
    if (userPercent >= rollout) {
      const control = (exp.variants || []).find(
        (v: Variant) => v.id === "control",
      );
      if (control) return control.id;
      return (exp.variants && exp.variants[0] && exp.variants[0].id) || null;
    }
    const variants = exp.variants || [];
    const totalWeight =
      variants.reduce((s: number, v: Variant) => s + (v.weight || 0), 0) ||
      variants.length;
    const pickHash = this.fnv1a32Hash(installId + "::" + salt + ":variant");
    let r = pickHash % totalWeight;
    for (const v of variants) {
      const w = v.weight || 0;
      if (r < w) return v.id;
      r -= w;
    }
    return variants.length ? variants[0].id : null;
  }

  private resolveConfigUrl(
    baseExperimentsUrl: string,
    configUrl: string,
  ): string | null {
    if (!configUrl) return null;
    try {
      const u = new URL(configUrl);
      return u.toString();
    } catch {
      try {
        const base = new URL(baseExperimentsUrl);
        if (configUrl.startsWith("/")) return base.origin + configUrl;
        const basePath = base.pathname.replace(/\/[^/]*$/, "/");
        return base.origin + basePath + configUrl;
      } catch {
        return null;
      }
    }
  }

  private async fetchJsonWithTimeout(
    url: string,
    timeoutMs = 5000,
  ): Promise<unknown> {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeoutMs);
    try {
      const res = await fetch(url, {
        cache: "no-store",
        signal: controller.signal,
      });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const j = await res.json();
      return j as unknown;
    } finally {
      clearTimeout(id);
    }
  }

  private async fetchAndCacheConfig(
    experimentId: string,
    variantId: string | null,
    configUrl: string | null,
  ): Promise<unknown | null> {
    if (!configUrl) return null;
    try {
      const cached = this.loadConfigFromPrefs(experimentId, variantId);
      if (cached) return cached;
      const url = this.resolveConfigUrl(
        this.experimentsUrl as string,
        configUrl,
      );
      if (!url) throw new Error("Invalid configUrl");
      const res = await fetch(url, { cache: "no-store" });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const config = await res.json();
      this.saveConfigToPrefs(experimentId, variantId, config as unknown);
      return config as unknown;
    } catch (e) {
      console.error(
        `Failed to fetch config for ${experimentId}/${variantId}: ${String(e)}`,
      );
      return null;
    }
  }

  // Public API
  async init(
    options: { installId?: string; timeoutMs?: number } = {},
  ): Promise<this> {
    const prefUrl = this.getPrefString(MANIFEST_URL_PREF, null);
    this.experimentsUrl = prefUrl || DEFAULT_EXPERIMENTS_URL;
    this.installId =
      options.installId || this.getPrefString(INSTALLID_PREF, null) || null;
    if (!this.installId) {
      const gen = Math.floor(Math.random() * 1e9) + "-" + Date.now();
      this.installId = gen;
      try {
        this.setPrefString(INSTALLID_PREF, this.installId);
      } catch (e) {
        console.error("Could not persist installId: " + String(e));
      }
    }
    this.assignments = this.loadAssignmentsFromPrefs();

    const timeoutMs = options.timeoutMs || 5000;
    try {
      const data = await this.fetchJsonWithTimeout(
        this.experimentsUrl as string,
        timeoutMs,
      );
      if (data && typeof data === "object" && !Array.isArray(data)) {
        const obj = data as { experiments?: unknown };
        if (Array.isArray(obj.experiments))
          this.experiments = obj.experiments as Experiment[];
        else this.experiments = [];
      } else this.experiments = [];
    } catch (e) {
      console.error(
        "Failed to fetch experiments.json, using cached assignments if present: " +
          String(e),
      );
      return this;
    }

    const now = this.nowUtc();
    let changed = false;
    for (const exp of this.experiments) {
      if (!this.isExperimentActive(exp)) {
        if (this.assignments[exp.id]) {
          delete this.assignments[exp.id];
          changed = true;
        }
        continue;
      }
      const prev = this.assignments[exp.id];
      if (prev && prev.installId === this.installId) continue;
      const variantId = this.chooseVariantForExperiment(
        exp,
        this.installId as string,
      );
      this.assignments[exp.id] = {
        installId: this.installId,
        variantId,
        assignedAt: now.toISOString(),
      };
      changed = true;
    }

    if (changed) this.saveAssignmentsToPrefs();

    for (const exp of this.experiments) {
      const a = this.assignments[exp.id];
      if (!a) continue;
      const variant = (exp.variants || []).find(
        (v: Variant) => v.id === a.variantId,
      );
      if (variant && variant.configUrl)
        this.fetchAndCacheConfig(exp.id, variant.id, variant.configUrl).then(
          (cfg) => {
            if (cfg) this.configs[exp.id] = cfg;
          },
        );
    }
    return this;
  }

  getVariant(experimentId: string): string | null {
    const a = this.assignments[experimentId];
    if (!a) return null;
    return a.variantId;
  }

  async getConfig(experimentId: string): Promise<unknown | null> {
    if (this.configs[experimentId]) return this.configs[experimentId];
    const a = this.assignments[experimentId];
    if (!a) return null;
    const exp = (this.experiments || []).find(
      (e: Experiment) => e.id === experimentId,
    );
    if (!exp) return null;
    const variant = (exp.variants || []).find(
      (v: Variant) => v.id === a.variantId,
    );
    if (!variant) return null;
    if (!variant.configUrl) return null;
    const cfg = await this.fetchAndCacheConfig(
      experimentId,
      variant.id,
      variant.configUrl,
    );
    if (cfg) this.configs[experimentId] = cfg;
    return cfg;
  }

  // --- convenience getters for tests / inspection ---
  /** Return the experiment object by id, or null */
  getExperimentById(experimentId: string): Experiment | null {
    return (
      (this.experiments || []).find((e: Experiment) => e.id === experimentId) ||
      null
    );
  }

  /** Return the Variant object by id. If variantId omitted, use current assignment. */
  getVariantById(
    experimentId: string,
    variantId?: string | null,
  ): Variant | null {
    const exp = this.getExperimentById(experimentId);
    if (!exp) return null;
    const vid =
      variantId ||
      (this.assignments[experimentId] &&
        this.assignments[experimentId].variantId) ||
      null;
    if (!vid) return null;
    return (exp.variants || []).find((v: Variant) => v.id === vid) || null;
  }

  /** Return stored assignment for an experiment (or null) */
  getAssignment(experimentId: string): Assignment | null {
    return this.assignments[experimentId] || null;
  }

  /** Return cached config from prefs. Does not perform network fetch. */
  getCachedConfig(
    experimentId: string,
    variantId?: string | null,
  ): unknown | null {
    const vid =
      variantId ||
      (this.assignments[experimentId] &&
        this.assignments[experimentId].variantId) ||
      null;
    if (!vid) return null;
    return this.loadConfigFromPrefs(experimentId, vid);
  }

  getAllAssignments(): Record<string, Assignment> {
    return Object.assign({}, this.assignments);
  }

  clearCache(): void {
    try {
      this.clearPref(ASSIGNMENTS_PREF);
    } catch {
      /* ignore */
    }
  }
}

export const Experiments = ExperimentsClient.getInstance();
export default Experiments;
