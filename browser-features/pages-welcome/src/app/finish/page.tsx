import Navigation from "../../components/Navigation.tsx";
import { useTranslation } from "react-i18next";
import { setDefaultBrowser } from "./dataManager.ts";
import { useState } from "react";
import { ArrowRight, ExternalLink } from "lucide-react";
import { useSetupReleaseNotesChoice } from "../../components/ReleaseNotesChoice.tsx";
import { Navigate } from "react-router-dom";
import { Button } from "../../../../../libs/ui/button.tsx";
import styles from "../../setup.module.css";
import { SetupSummary } from "../../components/SetupSummary.tsx";
import { SetupInfo } from "../../components/SetupControls.tsx";
export default function FinishPage() {
  const { t } = useTranslation();
  const { choice, reviewed } = useSetupReleaseNotesChoice();
  const [defaultStatus, setDefaultStatus] = useState<
    "idle" | "saving" | "success" | "error"
  >("idle");
  async function setAsDefaultBrowser() {
    setDefaultStatus("saving");
    try {
      setDefaultStatus(await setDefaultBrowser() ? "success" : "error");
    } catch (error) {
      console.error("[Welcome] Default browser update failed", error);
      setDefaultStatus("error");
    }
  }
  async function closeWelcomePage() {
    if (!await choice.confirm()) return;
    globalThis.open("about:newtab", "_blank");
    setTimeout(() => globalThis.close(), 50);
  }
  if (!reviewed) return <Navigate to="/support" replace />;
  return (
    <div>
      <h2>{t("setupV5.summaryTitle")}</h2>
      <SetupSummary />
      <section className={styles.section}>
        <h3>{t("setupV5.defaultTitle")}</h3>
        <p className={styles.muted}>{t("setupV5.defaultHint")}</p>
        <Button
          variant="secondary"
          disabled={defaultStatus === "saving"}
          onClick={() => void setAsDefaultBrowser()}
        >
          {t("finishPage.defaultBrowser")}
        </Button>
        {defaultStatus === "success" && (
          <p role="status">{t("finishPage.defaultBrowserSuccess")}</p>
        )}
        {defaultStatus === "error" && (
          <p role="alert" className={styles.error}>{t("ui.saveError")}</p>
        )}
        <div className={styles.links}>
          <a
            href="https://docs.floorp.app/docs/features/"
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("finishPage.support")}
            <ExternalLink size={16} aria-hidden="true" />
          </a>
          <a
            href="https://github.com/Floorp-Projects/Floorp"
            target="_blank"
            rel="noopener noreferrer"
          >
            GitHub<ExternalLink size={16} aria-hidden="true" />
          </a>
          <a
            href="https://twitter.com/floorp_browser"
            target="_blank"
            rel="noopener noreferrer"
          >
            X<ExternalLink size={16} aria-hidden="true" />
          </a>
        </div>
      </section>
      <SetupInfo>{t("setupV5.finishNotice")}</SetupInfo>
      {choice.error && (
        <p role="alert" className={styles.error}>{t("releaseNotes.error")}</p>
      )}
      <Navigation
        finalAction={
          <Button
            onClick={() => void closeWelcomePage()}
            disabled={!choice.ready || choice.saving}
          >
            {t("setupV5.getStarted")}
            <ArrowRight size={18} aria-hidden="true" />
          </Button>
        }
      />
    </div>
  );
}
