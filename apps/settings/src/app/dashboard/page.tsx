import { Avatar, AvatarFallback, AvatarImage } from "@/components/ui/avatar.tsx";
import { Button } from "@/components/ui/button.tsx";
import {
  Card,
  CardContent,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card.tsx";
import { useTranslation } from "react-i18next";

export default function Page() {
  const { t } = useTranslation();
  const accountImage = "https://example.com/avatar.png";
  const accountName = "User";

  return (
    <div className="py-2 space-y-6">
      <div className="flex flex-col items-start pl-6">
        <div className="m-5 p-1 rounded-full bg-gradient-to-tr from-blue-600 via-pink-600 to-orange-400">
          <div className="p-1 rounded-full bg-white">
            <Avatar alt="Avatar" className="w-20 h-20">
              <AvatarImage src={accountImage} />
              <AvatarFallback>{accountName}</AvatarFallback>
            </Avatar>
          </div>
        </div>
        <h1 className="text-3xl font-bold">
          {t("home.welcome", { name: accountName })}
        </h1>
        <p className="text-left text-sm mt-2">
          {t("home.description")}
        </p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 pl-6">
        <Card>
          <CardHeader>
            <CardTitle>{t("home.setup.title")}</CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm mb-2">
              {t("home.setup.setupDescription")}
            </p>
            <p className="font-bold text-sm mb-1">
              {t("home.setup.setupProgressTitle")}: 50%
            </p>
            <div className="w-full h-1 bg-blue-200 rounded">
              <div className="bg-blue-500 h-1 rounded" style={{ width: "25%" }}></div>
            </div>
            <p className="text-xs mt-1">
              {t("home.setup.step", { step: 2, total: 4 })}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a href="https://support.ablaze.one/setup">
                {t("home.setup.footerLinkText")}
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>
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
              <a href="https://support.mozilla.org/kb/enhanced-tracking-protection-firefox-desktop">
                {t("home.privacyAndTrackingProtection.footerLinkText")}
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>{t("home.ablazeAccount.title")}</CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.ablazeAccount.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a href="https://accounts.ablaze.one/signin">
                {t("home.ablazeAccount.footerLinkText")}
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>{t("home.manageExtensions.title")}</CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.manageExtensions.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a href="about:addons">
                {t("home.manageExtensions.footerLinkText")}
              </a>
            </Button>
          </CardFooter>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>{t("home.browserSupport.title")}</CardTitle>
          </CardHeader>
          <CardContent>
            <p className="text-sm">
              {t("home.browserSupport.description")}
            </p>
          </CardContent>
          <CardFooter>
            <Button asChild>
              <a href="https://docs.floorp.app/docs/features/">
                {t("home.browserSupport.footerLinkText")}
              </a>
            </Button>
          </CardFooter>
        </Card>
      </div>
    </div>
  );
}
