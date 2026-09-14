import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import { FeatureStories } from "./FeatureStories.tsx";
import styles from "./demos.module.css";
export default function FeaturesPage() {
  const { t } = useTranslation();
  return (
    <div className={styles.featuresPage} data-feature-fit>
      <h2>{t("setupV5.featuresTitle")}</h2>
      <FeatureStories />
      <Navigation />
    </div>
  );
}
