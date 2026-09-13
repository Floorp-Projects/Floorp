export const RELEASE_NOTES_SUPPORT_URL =
  "https://blog.floorp.app/ja/notice/260913-operational-transparency/";

export const RELEASE_NOTES_MODES = ["disabled", "blocking", "support"] as const;
export type ReleaseNotesMode = (typeof RELEASE_NOTES_MODES)[number];
export const RELEASE_NOTES_PREFS = {
  mode: "floorp.releaseNotes.mode",
  confirmed: "floorp.releaseNotes.choiceConfirmed",
  audience: "floorp.releaseNotes.audience",
  promptShown: "floorp.releaseNotes.promptShown",
} as const;

export function isReleaseNotesMode(value: unknown): value is ReleaseNotesMode {
  return RELEASE_NOTES_MODES.some((mode) => mode === value);
}

export function effectiveReleaseNotesMode(
  mode: unknown,
  confirmed: boolean | null,
): ReleaseNotesMode {
  return confirmed && isReleaseNotesMode(mode) ? mode : "blocking";
}

export function initialReleaseNotesChoice(
  mode: unknown,
  confirmed: boolean | null,
  audience: string | null,
): ReleaseNotesMode {
  return !confirmed && audience === "new"
    ? "support"
    : effectiveReleaseNotesMode(mode, confirmed);
}

export function releaseNotesAudience(
  isFirstRun: boolean,
  oldVersion: string | undefined,
  welcomeShown: boolean,
): "new" | "existing" {
  return isFirstRun && !oldVersion && !welcomeShown ? "new" : "existing";
}

export function shouldShowReleaseNotesChoice(
  audience: string,
  confirmed: boolean,
  promptShown: boolean,
): boolean {
  return audience === "existing" && !confirmed && !promptShown;
}
