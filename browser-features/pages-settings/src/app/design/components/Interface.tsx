import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useInterfaceDesigns } from "@/app/design/useInterfaceDesigns.ts";
import { useTranslation } from "react-i18next";
import { useFormContext, useWatch } from "react-hook-form";
import type { DesignFormData } from "@/types/pref.ts";
import styles from "../design.module.css";
import { Check } from "lucide-react";

export function Interface() {
  const { t } = useTranslation();
  const { control, setValue } = useFormContext<DesignFormData>();
  const design = useWatch({ control, name: "design" });
  const faviconColor = useWatch({ control, name: "faviconColor" });
  const options = useInterfaceDesigns();
  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle>{t("design.interface")}</CardTitle>
        <p className={styles.description}>{t("design.interfaceDescription")}</p>
      </CardHeader>
      <CardContent>
        <fieldset className={styles.choices}>
          <legend className={styles.srOnly}>{t("design.interface")}</legend>
          {options.map((option) => (
            <label
              key={option.value}
              className={styles.choice}
              data-selected={design === option.value}
            >
              <span className={styles.choiceMedia}>
                <img className={styles.choiceImage} src={option.image} alt="" />
              </span>
              <span className={styles.choiceLabel}>
                <input
                  type="radio"
                  className={styles.srOnly}
                  name="design"
                  value={option.value}
                  checked={design === option.value}
                  onChange={() => setValue("design", option.value)}
                />
                <span>{option.title}</span>
                <span className={styles.choiceIndicator} aria-hidden="true">
                  {design === option.value && (
                    <Check size={14} strokeWidth={2.5} />
                  )}
                </span>
              </span>
            </label>
          ))}
        </fieldset>
        <div className={styles.extra}>
          <h3>{t("design.otherInterfaceSettings")}</h3>
          <div className={styles.row}>
            <label htmlFor="favicon-color">
              {t("design.useFaviconColorToBackgroundOfNavigationBar")}
            </label>
            <Switch
              id="favicon-color"
              checked={!!faviconColor}
              onChange={(e) => setValue("faviconColor", e.target.checked)}
            />
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
