import {
  createContext,
  type ReactNode,
  useContext,
  useEffect,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { rpc } from "../lib/rpc/rpc.ts";
import {
  initialReleaseNotesChoice,
  RELEASE_NOTES_MODES,
  RELEASE_NOTES_PREFS,
  RELEASE_NOTES_SUPPORT_URL,
  type ReleaseNotesMode,
} from "../../../modules/common/release-notes.ts";

export function useReleaseNotesChoice(allowNewDefault = false) {
  const [mode, setMode] = useState<ReleaseNotesMode>("blocking");
  const [ready, setReady] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);

  useEffect(() => {
    let active = true;
    Promise.all([
      rpc.getStringPref(RELEASE_NOTES_PREFS.mode),
      rpc.getBoolPref(RELEASE_NOTES_PREFS.confirmed),
      rpc.getStringPref(RELEASE_NOTES_PREFS.audience),
    ]).then(([value, confirmed, audience]) => {
      if (!active) return;
      setMode(
        initialReleaseNotesChoice(
          value,
          confirmed,
          allowNewDefault ? audience : "existing",
        ),
      );
      setReady(true);
    }).catch(() => {
      if (active) setError(true);
    });
    return () => {
      active = false;
    };
  }, [allowNewDefault]);

  async function confirm() {
    if (!ready || saving) return false;
    setSaving(true);
    setError(false);
    try {
      await rpc.setStringPref(RELEASE_NOTES_PREFS.mode, mode);
      await rpc.setBoolPref(RELEASE_NOTES_PREFS.confirmed, true);
      return true;
    } catch {
      setError(true);
      return false;
    } finally {
      setSaving(false);
    }
  }

  return { mode, setMode, ready, saving, error, confirm };
}

const SetupReleaseNotesContext = createContext<
  {
    choice: ReturnType<typeof useReleaseNotesChoice>;
    reviewed: boolean;
    setReviewed: (reviewed: boolean) => void;
  } | null
>(null);

export function SetupReleaseNotesProvider(
  { children }: { children: ReactNode },
) {
  const choice = useReleaseNotesChoice(true);
  const [reviewed, setReviewed] = useState(false);
  return (
    <SetupReleaseNotesContext.Provider
      value={{ choice, reviewed, setReviewed }}
    >
      {children}
    </SetupReleaseNotesContext.Provider>
  );
}

export function useSetupReleaseNotesChoice() {
  const context = useContext(SetupReleaseNotesContext);
  if (!context) throw new Error("Missing SetupReleaseNotesProvider");
  return context;
}

export function ReleaseNotesChoice(
  { choice, setup = false }: {
    choice: ReturnType<typeof useReleaseNotesChoice>;
    setup?: boolean;
  },
) {
  const { t } = useTranslation();
  return (
    <section className="card border border-base-300 bg-base-100 w-full max-w-2xl mb-6">
      <div className="card-body">
        <h2 className="card-title">{t("releaseNotes.title")}</h2>
        <p className="text-sm opacity-80">{t("releaseNotes.description")}</p>
        <fieldset
          disabled={!choice.ready || choice.saving}
          className="space-y-4 my-3"
        >
          <legend className="sr-only">{t("releaseNotes.title")}</legend>
          {RELEASE_NOTES_MODES.map((mode) => (
            <label key={mode} className="flex items-start gap-3 cursor-pointer">
              <input
                type="radio"
                name="release-notes-choice"
                value={mode}
                checked={choice.mode === mode}
                onChange={() =>
                  choice.setMode(mode)}
                className="radio radio-primary radio-sm mt-1"
              />
              <span>
                <span className="block font-semibold">
                  {t(`releaseNotes.${mode}.label`)}
                </span>
                <span className="block text-sm opacity-80">
                  {t(`releaseNotes.${mode}.description`)}
                </span>
              </span>
            </label>
          ))}
        </fieldset>
        <p className="text-sm opacity-80">{t("releaseNotes.scope")}</p>
        <p className="text-sm">
          {t(
            setup
              ? "releaseNotes.applyOnFinish"
              : "releaseNotes.applyOnConfirm",
          )}
        </p>
        <a
          href={RELEASE_NOTES_SUPPORT_URL}
          target="_blank"
          rel="noopener noreferrer"
          className="link link-primary text-sm"
        >
          {t("releaseNotes.learnMore")}
        </a>
        {choice.error && (
          <p role="alert" className="text-error">{t("releaseNotes.error")}</p>
        )}
      </div>
    </section>
  );
}

export function ReleaseNotesPrompt() {
  const { t } = useTranslation();
  const choice = useReleaseNotesChoice();
  return (
    <main className="min-h-screen bg-base-100 text-base-content flex flex-col items-center justify-center p-6">
      <ReleaseNotesChoice choice={choice} />
      <div className="flex flex-wrap gap-3">
        <button
          type="button"
          className="btn btn-primary"
          disabled={!choice.ready || choice.saving}
          onClick={async () => {
            if (await choice.confirm()) globalThis.close();
          }}
        >
          {t("releaseNotes.confirm")}
        </button>
        <button
          type="button"
          className="btn btn-ghost"
          disabled={choice.saving}
          onClick={() => globalThis.close()}
        >
          {t("releaseNotes.dismiss")}
        </button>
      </div>
    </main>
  );
}
