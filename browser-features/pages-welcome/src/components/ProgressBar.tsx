import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";

const navigationItems = [
  { path: "/", labelKey: "navigation.welcome" },
  { path: "/localization", labelKey: "navigation.languageSupport" },
  { path: "/features", labelKey: "navigation.featureIntroduction" },
  { path: "/hub", labelKey: "navigation.hub" },
  { path: "/customize", labelKey: "navigation.initialSettings" },
  { path: "/support", labelKey: "navigation.support" },
  { path: "/finish", labelKey: "navigation.complete" },
];

export default function ProgressBar() {
  const { t } = useTranslation();
  const location = useLocation();
  const navigate = useNavigate();
  const currentIndex = navigationItems.findIndex((item) =>
    item.path === location.pathname
  );

  return (
    <div className="w-full mb-12">
      <div className="flex justify-between gap-4 md:hidden">
        <span>
          {t(navigationItems[currentIndex]?.labelKey ?? "navigation.welcome")}
        </span>
        <span>{currentIndex + 1} / {navigationItems.length}</span>
      </div>
      <div className="hidden md:block">
        <ul className="steps steps-horizontal w-full">
          {navigationItems.map((item, index) => (
            <li
              key={item.path}
              className={`step ${index <= currentIndex ? "step-primary" : ""}`}
              onClick={() => navigate(item.path)}
            >
              {t(item.labelKey)}
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
}
