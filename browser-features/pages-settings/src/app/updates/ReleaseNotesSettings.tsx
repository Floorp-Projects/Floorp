import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { rpc } from "@/lib/rpc/rpc.ts";
import {
  effectiveReleaseNotesMode,
  RELEASE_NOTES_PREFS,
  RELEASE_NOTES_SUPPORT_URL,
} from "../../../../modules/common/release-notes.ts";

const MODE_PREF = "floorp.releaseNotes.mode";
const MODES = ["disabled", "blocking", "support"] as const;
type Mode = (typeof MODES)[number];

export function ReleaseNotesSettings() {
  const { t } = useTranslation();
  const [mode, setMode] = useState<Mode>("blocking");
  const [ready, setReady] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);

  useEffect(() => {
    let active = true;
    Promise.all([
      rpc.getStringPref(MODE_PREF),
      rpc.getBoolPref(RELEASE_NOTES_PREFS.confirmed),
    ]).then(([value, confirmed]) => {
      if (!active) return;
      setMode(effectiveReleaseNotesMode(value, confirmed));
      setReady(true);
    }).catch(() => {
      if (active) setError(true);
    });
    return () => {
      active = false;
    };
  }, []);

  async function changeMode(value: Mode) {
    setSaving(true);
    setError(false);
    try {
      await rpc.setStringPref(MODE_PREF, value);
      await rpc.setBoolPref(RELEASE_NOTES_PREFS.confirmed, true);
      setMode(value);
    } catch {
      setError(true);
    } finally {
      setSaving(false);
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>{t("updates.releaseNotes.title")}</CardTitle>
        <CardDescription>
          {t("updates.releaseNotes.description")}
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <fieldset disabled={!ready || saving} className="space-y-3">
          <legend className="sr-only">{t("updates.releaseNotes.title")}</legend>
          {MODES.map((value) => (
            <label
              key={value}
              className="flex items-start gap-3 cursor-pointer"
            >
              <input
                type="radio"
                name="release-notes-mode"
                value={value}
                checked={mode === value}
                onChange={() => changeMode(value)}
                className="mt-1 accent-primary"
              />
              <span>
                <span className="block font-medium">
                  {t(`updates.releaseNotes.${value}.label`)}
                </span>
                <span className="block text-sm text-base-content/70">
                  {t(`updates.releaseNotes.${value}.description`)}
                </span>
              </span>
            </label>
          ))}
        </fieldset>
        <p className="text-sm text-base-content/70">
          {t("updates.releaseNotes.supportMessage")}
        </p>
        <p className="text-sm text-base-content/70">
          {t("updates.releaseNotes.scope")}
        </p>
        <a
          href={RELEASE_NOTES_SUPPORT_URL}
          target="_blank"
          rel="noopener noreferrer"
          className="link link-primary text-sm"
        >
          {t("updates.releaseNotes.learnMore")}
        </a>
        {error && (
          <p role="alert" className="text-sm text-error">
            {t("updates.releaseNotes.error")}
          </p>
        )}
      </CardContent>
    </Card>
  );
}
