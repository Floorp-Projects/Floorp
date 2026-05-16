import { useState } from "react";
import { useTranslation } from "react-i18next";
import { GraduationCap } from "lucide-react";
import { TutorialModal, type TutorialStep } from "./tutorial-modal.tsx";

interface LearnButtonProps {
  steps: TutorialStep[];
  title: string;
}

export function LearnButton({ steps, title }: LearnButtonProps) {
  const { t } = useTranslation();
  const [isOpen, setIsOpen] = useState(false);

  return (
    <>
      <button
        type="button"
        onClick={() => setIsOpen(true)}
        className="btn btn-sm btn-outline gap-2"
      >
        <GraduationCap size={16} />
        {t("tutorial.learnHowToUse")}
      </button>
      <TutorialModal
        isOpen={isOpen}
        onClose={() => setIsOpen(false)}
        steps={steps}
        title={title}
      />
    </>
  );
}
