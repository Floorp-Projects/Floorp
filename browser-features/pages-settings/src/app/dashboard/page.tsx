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
import {
  ExternalLink,
  HelpCircle,
  PuzzleIcon,
  Rocket,
  Shield,
  UserRound,
} from "lucide-react";

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

  function isLoggedInToMozillaAccount() {
    return homeData?.accountName !== null;
  }

  return (
    <div className="py-2 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <div className="m-5 p-1 rounded-full bg-gradient-to-tr from-blue-600 via-pink-600 to-orange-400">
          <div className="p-1 rounded-full bg-white">
            <Avatar className="w-20 h-20">
              <AvatarImage
                src={homeData?.accountImage}
                fallback={homeData?.accountName ?? t("home.defaultAccountName")}
              />
            </Avatar>
          </div>
        </div>
        <h1 className="text-3xl font-bold">
          {t("home.welcome", {
            name: homeData?.accountName ?? t("home.defaultAccountName"),
          })}
        </h1>
        <p className="text-left text-sm mt-2">
          {t("home.description")}
        </p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-x-4 pl-6">
        {
          /* <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Rocket className="size-5 text-blue-500" />
              {t("home.setup.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm mb-2">
              {t("home.setup.setupDescription")}
            </p>
            <p className="font-bold text-sm mb-1">
              {t("home.setup.setupProgressTitle")}: 50%
            </p>
            <div className="w-full h-1 bg-blue-200 rounded">
              <div className="bg-blue-500 h-1 rounded" style={{ width: "25%" }}>
              </div>
            </div>
            <p className="text-xs mt-1">
              {t("home.setup.step", { step: 2, total: 4 })}
            </p>
          </CardContent>
          <CardFooter>
            <a
              href="https://docs.floorp.app/docs/features/"
              className="flex items-center gap-2"
            >
              <Button>
                {t("home.setup.footerLinkText")}
                <ExternalLink className="size-4" />
              </Button>
            </a>
          </CardFooter>
        </Card> */
        }

        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Shield className="size-5 text-emerald-600" />
              {t("home.privacyAndTrackingProtection.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.privacyAndTrackingProtection.description")}
            </p>
          </CardContent>
          <CardFooter>
            <a
              href="https://support.mozilla.org/kb/enhanced-tracking-protection-firefox-desktop"
              className="flex items-center gap-2"
            >
              <Button>
                {t("home.privacyAndTrackingProtection.footerLinkText")}
                <ExternalLink className="size-4" />
              </Button>
            </a>
          </CardFooter>
        </Card>

        <Card className="z-1 hidden">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <UserRound className="size-5 text-orange-500" />
              {t("home.ablazeAccount.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.ablazeAccount.description")}
            </p>
          </CardContent>
          <CardFooter>
            <a
              href="https://accounts.ablaze.one/signin"
              className="flex items-center gap-2"
            >
              <Button>
                {t("home.ablazeAccount.footerLinkText")}
                <ExternalLink className="size-4" />
              </Button>
            </a>
          </CardFooter>
        </Card>

        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <PuzzleIcon className="size-5 text-purple-500" />
              {t("home.manageExtensions.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.manageExtensions.description")}
            </p>
          </CardContent>
          <CardFooter>
            <a href="about:addons" className="flex items-center gap-2">
              <Button>
                {t("home.manageExtensions.footerLinkText")}
                <ExternalLink className="size-4" />
              </Button>
            </a>
          </CardFooter>
        </Card>

        <Card className="z-1">
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <HelpCircle className="size-5 text-blue-500" />
              {t("home.browserSupport.title")}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.browserSupport.description")}
            </p>
          </CardContent>
          <CardFooter>
            <a
              href="https://docs.floorp.app/docs/features/"
              className="flex items-center gap-2"
            >
              <Button>
                {t("home.browserSupport.footerLinkText")}
                <ExternalLink className="size-4" />
              </Button>
            </a>
          </CardFooter>
        </Card>
      </div>
    </div>
  );
}
