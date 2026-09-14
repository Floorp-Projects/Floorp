import { Avatar, AvatarImage } from "@/components/common/avatar.tsx";
import { Button } from "@/components/common/button.tsx";
import {
  Card,
  CardContent,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useHomeData } from "./dataManager.ts";
import { useEffect, useState } from "react";
import type { HomeData } from "@/types/pref";
import { ExternalLink, HelpCircle, PuzzleIcon, Shield } from "lucide-react";

export default function Page() {
  const { t } = useTranslation();
  const [homeData, setHomeData] = useState<HomeData | null>(null);

  useEffect(() => {
    async function fetchHomeData() {
      const data = await useHomeData();
      setHomeData(data);
    }
    fetchHomeData();
  }, []);

  return (
    <div className="floorp-settings-page">
      <div className="floorp-page-header">
        <div className="floorp-account-avatar">
          <div className="floorp-account-avatar-image">
            <Avatar className="w-20 h-20">
              <AvatarImage
                src={homeData?.accountImage}
                fallback={homeData?.accountName ?? t("home.defaultAccountName")}
              />
            </Avatar>
          </div>
        </div>
        <h1 className="floorp-page-heading">
          {t("home.welcome", {
            name: homeData?.accountName ?? t("home.defaultAccountName"),
          })}
        </h1>
        <p className="floorp-page-description">
          {t("home.description")}
        </p>
      </div>

      <div className="floorp-settings-sections">
        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Shield className="size-5 text-primary" />
              {t("home.privacyAndTrackingProtection.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.privacyAndTrackingProtection.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a
                href="https://support.mozilla.org/kb/enhanced-tracking-protection-firefox-desktop"
                target="_blank"
                className="flex items-center gap-2"
              >
                {t("home.privacyAndTrackingProtection.footerLinkText")}
                <ExternalLink className="size-4" />
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <PuzzleIcon className="size-5 text-primary" />
              {t("home.manageExtensions.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.manageExtensions.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a
                href="about:addons"
                target="_blank"
                className="flex items-center gap-2"
              >
                {t("home.manageExtensions.footerLinkText")}
                <ExternalLink className="size-4" />
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <HelpCircle className="size-5 text-primary" />
              {t("home.browserSupport.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.browserSupport.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a
                href="https://docs.floorp.app/docs/features/"
                target="_blank"
                className="flex items-center gap-2"
              >
                {t("home.browserSupport.footerLinkText")}
                <ExternalLink className="size-4" />
              </a>
            </Button>
          </CardFooter>
        </Card>
      </div>
    </div>
  );
}
