import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { welcomeSteps } from "./steps.ts";
import styles from "../setup.module.css";
import { NativeSelect } from "@chakra-ui/react";
import { ChevronDown } from "lucide-react";
export default function ProgressBar() {
  const { t } = useTranslation();
  const { pathname } = useLocation();
  const navigate = useNavigate();
  return (
    <nav
      className={styles.progress}
      aria-label={t("ui.setupProgress", { defaultValue: "Setup progress" })}
    >
      <NativeSelect.Root unstyled className={styles.compactProgress}>
        <NativeSelect.Field
          aria-label={t("ui.stepNavigation", {
            defaultValue: "Setup navigation",
          })}
          value={pathname}
          onChange={(event) => navigate(event.target.value)}
          className={styles.select}
        >
          {welcomeSteps.map((item, index) => (
            <option key={item.path} value={item.path}>
              {index + 1}. {t(`setupV5.steps.${index}`)}
            </option>
          ))}
        </NativeSelect.Field>
        <NativeSelect.Indicator className={styles.selectIndicator}>
          <ChevronDown size={18} aria-hidden="true" />
        </NativeSelect.Indicator>
      </NativeSelect.Root>
      <ol className={styles.steps}>
        {welcomeSteps.map((item) => (
          <li key={item.path}>
            <button
              type="button"
              aria-current={pathname === item.path ? "step" : undefined}
              onClick={() => navigate(item.path)}
            >
              {t(`setupV5.steps.${welcomeSteps.indexOf(item)}`)}
            </button>
          </li>
        ))}
      </ol>
    </nav>
  );
}
