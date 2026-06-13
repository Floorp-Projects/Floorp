import { useCallback } from "react";
import { useTranslation } from "react-i18next";
import { GraduationCap } from "lucide-react";
import { rpc } from "../../lib/rpc/rpc.ts";

interface LearnButtonProps {
  tourId: string;
  labelKey?: string;
}

export function LearnButton(
  { tourId, labelKey = "tutorial.learnHowToUse" }: LearnButtonProps,
) {
  const { t } = useTranslation();

  const handleStartTour = useCallback(async () => {
    const payload = JSON.stringify({ tourId });
    try {
      await rpc.setStringPref("floorp.guidedTour.request", payload);
    } catch (error) {
      console.error("[LearnButton] Failed to start tour", error);
    }
  }, [tourId]);

  return (
    <button
      type="button"
      onClick={handleStartTour}
      className="btn btn-sm btn-outline gap-2"
    >
      <GraduationCap size={16} />
      {t(labelKey)}
    </button>
  );
}
