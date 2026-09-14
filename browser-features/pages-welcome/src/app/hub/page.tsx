import { useTranslation } from "react-i18next";
import { ExternalLink, Layers, Mouse, PanelsTopLeft } from "lucide-react";
import Navigation from "../../components/Navigation.tsx";
import { Button } from "../../../../../libs/ui/button.tsx";
import styles from "../../setup.module.css";
const icons = [PanelsTopLeft, Layers, Mouse];
export default function HubIntroPage() {
  const { t } = useTranslation();
  return (
    <div>
      <h2>{t("setupV5.hubTitle")}</h2>
      <div className={styles.storyList}>
        {icons.map((Icon, i) => (
          <div key={i} className={styles.storyRow}>
            <span className={styles.storyIcon}>
              <Icon size={32} aria-hidden="true" />
            </span>
            <div>
              <h3>{t(`setupV5.hubItems.${i}.title`)}</h3>
              <p>{t(`setupV5.hubItems.${i}.description`)}</p>
            </div>
          </div>
        ))}
      </div>
      <div className={styles.links}>
        <Button
          variant="secondary"
          onClick={() => globalThis.open("about:hub", "_blank")}
        >
          {t("setupV5.openHub")}
          <ExternalLink size={18} aria-hidden="true" />
        </Button>
      </div>
      <p className={styles.muted}>{t("setupV5.newTab")}</p>
      <div className={styles.links}>
        <a href="about:preferences" target="_blank" rel="noopener noreferrer">
          {t("setupV5.openPreferences")}
          <ExternalLink size={16} aria-hidden="true" />
        </a>
      </div>
      <p className={styles.muted}>{t("setupV5.hubAddress")}</p>
      <Navigation />
    </div>
  );
}
