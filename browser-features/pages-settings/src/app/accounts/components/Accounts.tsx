import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Avatar, AvatarImage } from "@/components/common/avatar.tsx";
import { useTranslation } from "react-i18next";
import type { AccountsFormData } from "@/types/pref.ts";
import { ExternalLink, Settings } from "lucide-react";

type AccountsProps = {
  accountAndProfileData: AccountsFormData | null;
};

export function Accounts({ accountAndProfileData }: AccountsProps) {
  const { t } = useTranslation();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Settings className="size-5" />
          {t("accounts.mozillaAccountSyncSettings")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="flex items-center gap-4">
          <Avatar>
            <AvatarImage
              src={accountAndProfileData?.accountImage}
              alt={accountAndProfileData?.accountInfo.displayName ??
                accountAndProfileData?.accountInfo.email ??
                t("accounts.notLoggedIn")}
            />
          </Avatar>
          <div className="space-y-1">
            <p className="font-medium">
              {accountAndProfileData?.accountInfo.displayName ??
                accountAndProfileData?.accountInfo.email ??
                t("accounts.notLoggedIn")}
            </p>
            <p className="text-base-content/70">
              {accountAndProfileData?.accountInfo.email
                ? t("accounts.syncedTo", {
                  email: accountAndProfileData?.accountInfo.email,
                })
                : t("accounts.notSynced")}
            </p>
          </div>
        </div>

        <a
          href="https://accounts.firefox.com/signin"
          target="_blank"
          className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
        >
          {t("accounts.manageMozillaAccount")}
          <ExternalLink className="size-4" />
        </a>

        <a
          href="#"
          onClick={(e) => {
            e.preventDefault();
            globalThis.NRAddTab("about:preferences#sync");
          }}
          className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
        >
          {t("accounts.manageFirefoxFeatureSync")}
          <ExternalLink className="size-4" />
        </a>
      </CardContent>
    </Card>
  );
}
