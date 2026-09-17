import styles from "@/components/common/settings-sections.module.css";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card";
import { useTranslation } from "react-i18next";
import type { AccountsFormData } from "@/types/pref";
import { ExternalLink, User } from "lucide-react";
import { useState } from "react";
import { openCurrentProfileDirectory } from "../dataManager.ts";

type ProfileProps = {
  accountAndProfileData: AccountsFormData | null;
};

export function Profile({ accountAndProfileData }: ProfileProps) {
  const { t } = useTranslation();
  const [openError, setOpenError] = useState(false);

  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <User className="size-5" />
          {t("accounts.profileManagement")}
        </CardTitle>
      </CardHeader>
      <CardContent className={`${styles.details} space-y-6`}>
        <p className="text-base-content/90">
          {t("accounts.profileManagementDescription")}
        </p>

        <div className="space-y-2">
          <p className="text-base-content/90">
            {t("accounts.currentProfileName", {
              name: accountAndProfileData?.profileName,
            })}
          </p>

          <p className="text-base-content/90">
            {t("accounts.profileSaveLocation", {
              path: accountAndProfileData?.profileDir,
            })}
          </p>
        </div>

        <div className={styles.actions}>
          <a
            href="#"
            onClick={(e) => {
              e.preventDefault();
              globalThis.NRAddTab("about:profiles");
            }}
            className={styles.link}
          >
            {t("accounts.openProfileManager")}
            <ExternalLink className="size-4" />
          </a>
          <a
            href="#"
            onClick={async (event) => {
              event.preventDefault();
              try { setOpenError(!await openCurrentProfileDirectory()); }
              catch { setOpenError(true); }
            }}
            className={styles.link}
          >
            {t("accounts.openProfileSaveLocation")}
            <ExternalLink className="size-4" />
          </a>
        </div>
        {openError && <p role="alert" className="floorp-notice floorp-notice-error">{t("ui.loadError")}</p>}
      </CardContent>
    </Card>
  );
}
