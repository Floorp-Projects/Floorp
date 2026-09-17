import { useEffect } from "react";
import { useTranslation } from "react-i18next";
import { ArrowRight } from "lucide-react";
import { Button } from "../../../../../libs/ui/button.tsx";
import { FloorpBrand } from "../../../../../libs/ui/brand.tsx";
import { LanguageSettings } from "../localization/LanguageSettings.tsx";
import { FeatureStories } from "../features/FeatureStories.tsx";
import styles from "../../welcome.module.css";

export default function WhatsNewPage() {
  const { t } = useTranslation();
  const version =
    new URL(globalThis.location.href).searchParams.get("upgrade") ?? "12";
  useEffect(() => {
    const previousTitle = document.title;
    document.title = t("whatsNew.documentTitle", {
      version,
      defaultValue: "Floorp – What's New",
    });
    return () => {
      document.title = previousTitle;
    };
  }, [t, version]);
  function closePage() {
    globalThis.open("about:newtab", "_blank");
    setTimeout(() => globalThis.close(), 50);
  }
  return (
    <div className={styles.shell}>
      <a className="floorp-skip" href="#updates-content">
        {t("ui.skipToContent")}
      </a>
      <header className={styles.updateHeader}>
        <div className={styles.brand}>
          <FloorpBrand onDark />
        </div>
        <div className={styles.updateTitle}>
          <h1 className="floorp-page-heading">
            {t("whatsNew.title", { version })}
          </h1>
          <p className="floorp-page-description">{t("whatsNew.subtitle")}</p>
        </div>
      </header>
      <main id="updates-content" tabIndex={-1} className={styles.content}>
        <LanguageSettings />
        <section className={styles.section}>
          <h2>{t("hubPage.sectionTitle")}</h2>
          <p>{t("hubPage.description")}</p>
          <ul className={styles.bulletList}>
            {(["dedicated", "accessible", "centralized"] as const).map((
              key,
            ) => <li key={key}>{t(`hubPage.features.${key}`)}</li>)}
          </ul>
          <div className={styles.actions}>
            <Button onClick={() => globalThis.open("about:hub", "_blank")}>
              {t("hubPage.openHub")}
            </Button>
          </div>
        </section>
        <FeatureStories />
        <div className={styles.actions}>
          <Button onClick={closePage}>
            {t("finishPage.getStarted")}
            <ArrowRight size={18} aria-hidden="true" />
          </Button>
        </div>
      </main>
      <footer className={styles.updateFooter}>
        <h2>{t("whatsNew.footer.follow")}</h2>
        <div className={styles.links}>
          <a
            href="https://github.com/Floorp-Projects/Floorp"
            target="_blank"
            rel="noopener noreferrer"
          >
            GitHub
          </a>
          <a
            href="https://twitter.com/floorp_browser"
            target="_blank"
            rel="noopener noreferrer"
          >
            X (Twitter)
          </a>
          <a
            href="https://floorp.app/discord"
            target="_blank"
            rel="noopener noreferrer"
          >
            Discord
          </a>
        </div>
        <div className={styles.links}>
          <a
            href="https://floorp.app/terms"
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("whatsNew.footer.terms")}
          </a>
          <a
            href="https://floorp.app/privacy"
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("whatsNew.footer.privacy")}
          </a>
          <a
            href="https://docs.floorp.app"
            target="_blank"
            rel="noopener noreferrer"
          >
            {t("whatsNew.footer.about")}
          </a>
        </div>
        <p>
          {t("whatsNew.footer.copyright", { year: new Date().getFullYear() })}
        </p>
      </footer>
    </div>
  );
}
