import { useEffect } from "react";
import { RadioCard } from "@chakra-ui/react";
import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { useSetupReleaseNotesChoice } from "../../components/ReleaseNotesChoice.tsx";
import { SelectionBadge, SetupInfo } from "../../components/SetupControls.tsx";
import {
  RELEASE_NOTES_MODES,
  RELEASE_NOTES_SUPPORT_URL,
} from "../../../../modules/common/release-notes.ts";
import styles from "../../setup.module.css";
export default function SupportPage() {
  const { t } = useTranslation();
  const { choice, setReviewed } = useSetupReleaseNotesChoice();
  useEffect(() => {
    if (choice.ready) setReviewed(true);
  }, [choice.ready, setReviewed]);
  return (
    <div>
      <h2>{t("setupV5.supportTitle")}</h2>
      {!choice.ready && !choice.error && <p role="status">{t("ui.loading")}</p>}
      <RadioCard.Root
        unstyled
        value={choice.mode}
        disabled={!choice.ready || choice.saving}
        aria-label={t("setupV5.supportTitle")}
        className={styles.radioStack}
        onValueChange={({ value }) => {
          const mode = RELEASE_NOTES_MODES.find((mode) => mode === value);
          if (mode) choice.setMode(mode);
        }}
      >
        {RELEASE_NOTES_MODES.map((mode) => (
          <RadioCard.Item key={mode} value={mode} className={styles.radioItem}>
            <RadioCard.ItemHiddenInput />
            <RadioCard.ItemControl className={styles.radioControl}>
              <RadioCard.ItemContent>
                <RadioCard.ItemText>
                  {t(`setupV5.supportLabels.${mode}`)}
                  {choice.mode === mode && <SelectionBadge />}
                </RadioCard.ItemText>
                <RadioCard.ItemDescription className={styles.radioDescription}>
                  {t(`releaseNotes.${mode}.description`)}
                </RadioCard.ItemDescription>
              </RadioCard.ItemContent>
            </RadioCard.ItemControl>
          </RadioCard.Item>
        ))}
      </RadioCard.Root>
      <SetupInfo>
        <strong>{t("setupV5.scopeTitle")}</strong>
        <p>{t("releaseNotes.scope")}</p>
        <a
          className={styles.muted}
          href={RELEASE_NOTES_SUPPORT_URL}
          target="_blank"
          rel="noopener noreferrer"
        >
          {t("releaseNotes.learnMore")}
        </a>
      </SetupInfo>
      <p className={styles.muted}>{t("setupV5.supportNotice")}</p>
      {choice.error && (
        <p role="alert" className={styles.error}>{t("releaseNotes.error")}</p>
      )}
      <Navigation />
    </div>
  );
}
