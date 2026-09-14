import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { WelcomeScene } from "./WelcomeScene.tsx";
export default function WelcomePage() {
  const { t } = useTranslation();
  return (
    <div>
      <h2>{t("setupV5.startTitle")}</h2>
      <WelcomeScene />
      <Navigation />
    </div>
  );
}
