import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { ArrowLeft, ArrowRight } from "lucide-react";
import { Button } from "../../../../libs/ui/button.tsx";
import { welcomeSteps } from "./steps.ts";
import styles from "../setup.module.css";
import type { NavigationProps } from "./types.ts";
export default function Navigation({ finalAction }: NavigationProps) {
  const { t } = useTranslation();
  const { pathname } = useLocation();
  const navigate = useNavigate();
  const index = welcomeSteps.findIndex((item) => item.path === pathname);
  return (
    <nav
      className={styles.navigation}
      aria-label={t("ui.stepNavigation", { defaultValue: "Setup navigation" })}
    >
      {index > 0 && (
        <Button
          variant="secondary"
          onClick={() => navigate(welcomeSteps[index - 1].path)}
        >
          <ArrowLeft size={18} aria-hidden="true" />
          {t("setupV5.back")}
        </Button>
      )}
      {finalAction ?? (index < welcomeSteps.length - 1 && (
        <Button
          onClick={() => navigate(welcomeSteps[index + 1].path)}
        >
          {t(index === 0 ? "setupGuide.start" : "navigation.next")}
          <ArrowRight size={18} aria-hidden="true" />
        </Button>
      ))}
    </nav>
  );
}
