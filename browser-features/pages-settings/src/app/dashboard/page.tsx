import { Link } from "react-router-dom";
import { useEffect, useState } from "react";
import { Avatar, AvatarImage } from "@/components/common/avatar.tsx";
import type { HomeData } from "@/types/pref.ts";
import { useHomeData } from "./dataManager.ts";
import { useTranslation } from "react-i18next";
import {
  ChevronRight,
  CircleHelp,
  ExternalLink,
  Puzzle,
  Shield,
} from "lucide-react";
import { SettingPreview } from "./SettingPreview.tsx";
import styles from "./home.module.css";

const destinations = [
  { key: "tabAndAppearance", path: "design" },
  { key: "workspaces", path: "workspaces" },
  { key: "browserSidebar", path: "sidebar" },
  { key: "mouseGesture", path: "gesture" },
  { key: "keyboardShortcuts", path: "shortcuts" },
  { key: "performance", path: "performance" },
];
const resources = [
  {
    key: "privacy",
    icon: Shield,
    href:
      "https://support.mozilla.org/kb/enhanced-tracking-protection-firefox-desktop",
  },
  { key: "extensions", href: "about:addons", icon: Puzzle },
  {
    key: "help",
    href: "https://docs.floorp.app/docs/features/",
    icon: CircleHelp,
  },
];
export default function Page() {
  const { t } = useTranslation();
  const [homeData, setHomeData] = useState<HomeData | null>(null);
  useEffect(() => {
    let active = true;
    useHomeData().then((data) => {
      if (active) setHomeData(data);
    }).catch((error) =>
      console.error("[Settings:home] Account loading failed", error)
    );
    return () => {
      active = false;
    };
  }, []);
  const accountName = homeData?.accountName ?? t("home.defaultAccountName");
  return (
    <div className={styles.home}>
      <header className={styles.greeting}>
        <Avatar className={styles.avatar}>
          <AvatarImage
            src={homeData?.accountImage ??
              "chrome://browser/skin/fxa/avatar-color.svg"}
            alt=""
            fallback={accountName.charAt(0)}
          />
        </Avatar>
        <div>
          <h1 className="floorp-page-heading">
            {t("home.welcome", { name: accountName })}
          </h1>
          <p className="floorp-page-description">
            {t("settingsHome.description")}
          </p>
        </div>
      </header>
      <section aria-labelledby="settings-destinations">
        <h2 id="settings-destinations">{t("settingsHome.title")}</h2>
        <div className={styles.destinations}>
          {destinations.map(({ key, path }) => (
            <Link
              key={key}
              to={`/features/${path}`}
              className={styles.destination}
            >
              <SettingPreview kind={path} />
              <span className={styles.destinationCopy}>
                <strong>
                  {t(`pages.${key}`)}
                  <ChevronRight size={20} aria-hidden="true" />
                </strong>
                <small>{t(`settingsHome.descriptions.${key}`)}</small>
              </span>
            </Link>
          ))}
        </div>
      </section>
      <section aria-labelledby="settings-resources">
        <h2 id="settings-resources">{t("settingsHome.manage")}</h2>
        <div className={styles.resources}>
          {resources.map(({ key, href, icon: Icon }) => (
            <a key={key} href={href} target="_blank" rel="noopener noreferrer">
              <Icon size={24} aria-hidden="true" />
              <span>{t(`settingsHome.resources.${key}`)}</span>
              <ExternalLink size={18} aria-hidden="true" />
            </a>
          ))}
        </div>
      </section>
    </div>
  );
}
