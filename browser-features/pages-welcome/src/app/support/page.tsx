import { useEffect } from "react";
import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import {
  ReleaseNotesChoice,
  useSetupReleaseNotesChoice,
} from "../../components/ReleaseNotesChoice.tsx";

export default function SupportPage() {
  const { t } = useTranslation();
  const { choice, setReviewed } = useSetupReleaseNotesChoice();

  useEffect(() => {
    if (choice.ready) setReviewed(true);
  }, [choice.ready, setReviewed]);

  return (
    <main className="flex flex-col items-center py-6">
      <h1 className="text-3xl md:text-4xl font-bold text-center mb-8">
        {t("navigation.support")}
      </h1>
      <ReleaseNotesChoice choice={choice} setup />
      <Navigation />
    </main>
  );
}
