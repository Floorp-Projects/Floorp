import { Button } from "../../../../libs/ui/button.tsx";
import styles from "../welcome.module.css";
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
    ]).then(([value, confirmed]) => {
      if (!active) return;
      setMode(
        initialReleaseNotesChoice(
          value,
          confirmed,
          allowNewDefault ? "new" : "existing",
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
  { choice, setup = false, standalone = false }: {
    choice: ReturnType<typeof useReleaseNotesChoice>;
    setup?: boolean;
    standalone?: boolean;
  },
) {
  const { t } = useTranslation();
  const Heading = standalone ? "h1" : "h2";
  return (
    <section className={styles.section}>
      <div className={styles.choiceContent}>
        <Heading className={standalone ? "floorp-page-heading" : undefined}>{t("releaseNotes.title")}</Heading>
        <p className="text-sm opacity-80">{t("releaseNotes.description")}</p>
        <fieldset
          disabled={!choice.ready || choice.saving}
          className="space-y-4 my-3"
        >
          <legend className="sr-only">{t("releaseNotes.title")}</legend>
          {RELEASE_NOTES_MODES.map((mode) => (
            <label key={mode} className={styles.choice}>
              <input
                type="radio"
                name="release-notes-choice"
                value={mode}
                checked={choice.mode === mode}
                onChange={() =>
                  choice.setMode(mode)}
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
          className={styles.textLink}
        >
          {t("releaseNotes.learnMore")}
        </a>
        {choice.error && (
          <p role="alert" className={styles.error}>{t("releaseNotes.error")}</p>
        )}
      </div>
    </section>
  );
}

export function ReleaseNotesPrompt() {
  const { t } = useTranslation();
  const choice = useReleaseNotesChoice();
  return (
    <main className={styles.content}>
      <ReleaseNotesChoice choice={choice} standalone />
      <div className="flex flex-wrap gap-3">
        <Button
          type="button"
          disabled={!choice.ready || choice.saving}
          onClick={async () => {
            if (await choice.confirm()) globalThis.close();
          }}
        >
          {t("releaseNotes.confirm")}
        </Button>
        <Button
          type="button"
          variant="ghost"
          disabled={choice.saving}
          onClick={() => globalThis.close()}
        >
          {t("releaseNotes.dismiss")}
        </Button>
      </div>
    </main>
  );
}
