import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { ExternalLink, Rows } from "lucide-react";

export function Tabbar() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Rows className="size-5" />
          {t("design.tabBar")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div>
          <label className="block mb-2 text-base font-medium">
            {t("design.style")}
          </label>
          <p className="text-sm text-muted-foreground mb-4">
            {t("design.styleDescription")}
          </p>
          <div className="flex space-x-4">
            <label className="flex items-center space-x-2">
              <input
                type="radio"
                name="style"
                value="horizontal"
                checked={getValues("style") === "horizontal"}
                onChange={(e) => setValue("style", e.target.value)}
              />
              <span>{t("design.horizontal")}</span>
            </label>
            <label className="flex items-center space-x-2">
              <input
                type="radio"
                name="style"
                value="multirow"
                checked={getValues("style") === "multirow"}
                onChange={(e) => setValue("style", e.target.value)}
              />
              <span>{t("design.multirow")}</span>
            </label>
          </div>
          {getValues("style") === "multirow" && (
            <div className="flex flex-col gap-y-2 mt-6">
              <label className="flex items-center space-x-2 cursor-pointer">
                <input
                  type="checkbox"
                  name="maxRowEnabled"
                  checked={getValues("maxRowEnabled")}
                  onChange={(e) => setValue("maxRowEnabled", e.target.checked)}
                  className="accent-primary rounded border border-primary focus:ring-2 focus:ring-primary"
                  style={{ width: 18, height: 18 }}
                />
                <span className="text-sm">{t("design.maxRowEnabled")}</span>
              </label>
              <div className="flex flex-col">
                <label className="block mb-1">
                  {t("design.maxRow")}
                  <span className="float-right text-xs text-muted-foreground">
                    1 ~ 10
                  </span>
                </label>
                <input
                  type="number"
                  name="maxRow"
                  value={getValues("maxRow")}
                  onChange={(e) =>
                    setValue("maxRow", Number.parseInt(e.target.value))}
                  className="bg-background border border-input rounded px-3 py-2 w-full text-base focus:outline-none focus:ring-2 focus:ring-primary"
                  min={1}
                  max={10}
                  disabled={!getValues("maxRowEnabled")}
                />
              </div>
            </div>
          )}
        </div>

        <div className="alert alert-warning alert-soft">
          <span className="text-sm">
            {t("design.verticalTabIsRemovedFromBrowser")}{" "}
            <a
              href="https://docs.floorp.app/docs/features/about-vertical-tab-bar"
              target="_blank"
              className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
            >
              {t("design.learnMore")}
              <ExternalLink className="size-4" />
            </a>
          </span>
        </div>

        <div className="mt-4">
          <label className="block mb-2 text-base font-medium">
            {t("design.position")}
          </label>
          <p className="text-sm text-muted-foreground mb-4">
            {t("design.positionDescription")}
          </p>
          <div className="flex flex-col space-y-2">
            {[
              { value: "default", label: "design.default" },
              { value: "hide-horizontal-tabbar", label: "design.hideTabBar" },
              {
                value: "optimise-to-vertical-tabbar",
                label: "design.hideTabBarAndTitleBar",
              },
              {
                value: "bottom-of-navigation-toolbar",
                label: "design.showTabBarOnBottomOfNavigationBar",
              },
              {
                value: "bottom-of-window",
                label: "design.showTabBarOnBottomOfWindow",
              },
            ].map(({ value, label }) => (
              <label key={value} className="flex items-center space-x-2">
                <input
                  type="radio"
                  name="position"
                  value={value}
                  checked={getValues("position") === value}
                  onChange={(e) =>
                    setValue("position", e.target.value)}
                />
                <span>{t(label)}</span>
              </label>
            ))}
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
