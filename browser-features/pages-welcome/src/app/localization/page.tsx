import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { LanguageSettings } from "./LanguageSettings.tsx";
export default function LocalizationPage() {
  const { t } = useTranslation();
  return (
    <div>
      <h2>{t("setupV5.languageTitle")}</h2>
      <LanguageSettings />
      <Navigation />
    </div>
  );
}
