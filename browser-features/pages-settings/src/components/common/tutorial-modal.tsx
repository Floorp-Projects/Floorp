import { useState, useCallback, useEffect, useRef } from "react";
import { useTranslation } from "react-i18next";
import { X, ChevronLeft, ChevronRight } from "lucide-react";

export interface TutorialStep {
  titleKey: string;
  descriptionKey: string;
  image?: React.ReactNode;
  content?: React.ReactNode;
}

interface TutorialModalProps {
  isOpen: boolean;
  onClose: () => void;
  steps: TutorialStep[];
  title: string;
}

export function TutorialModal({ isOpen, onClose, steps, title }: TutorialModalProps) {
  const { t } = useTranslation();
  const [currentStep, setCurrentStep] = useState(0);
  const [mounted, setMounted] = useState(false);
  const [render, setRender] = useState(false);
  const closeTimerRef = useRef<ReturnType<typeof setTimeout>>(null);

  useEffect(() => {
    if (isOpen) {
      setRender(true);
      requestAnimationFrame(() => setMounted(true));
    } else {
      setMounted(false);
      closeTimerRef.current = setTimeout(() => {
        setRender(false);
        setCurrentStep(0);
      }, 200);
    }
    return () => {
      if (closeTimerRef.current) clearTimeout(closeTimerRef.current);
    };
  }, [isOpen]);

  const handleNext = useCallback(() => {
    if (currentStep < steps.length - 1) {
      setCurrentStep((prev) => prev + 1);
    }
  }, [currentStep, steps.length]);

  const handlePrev = useCallback(() => {
    if (currentStep > 0) {
      setCurrentStep((prev) => prev - 1);
    }
  }, [currentStep]);

  const handleClose = useCallback(() => {
    onClose();
  }, [onClose]);

  if (!render) return null;

  const step = steps[currentStep];

  return (
    <div className={`fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm transition-opacity duration-200 ${mounted ? "opacity-100" : "opacity-0"}`}>
      <div className={`bg-base-100 rounded-2xl shadow-2xl ${step.content ? "max-w-2xl" : "max-w-lg"} w-full mx-4 overflow-hidden transition-all duration-200 ${mounted ? "scale-100 translate-y-0" : "scale-95 translate-y-2"}`}>
        <div className="flex items-center justify-between p-4 border-b border-base-300">
          <div>
            <h2 className="text-lg font-bold">{title}</h2>
            <p className="text-sm text-base-content/60">
              {t("tutorial.stepOf", { current: currentStep + 1, total: steps.length })}
            </p>
          </div>
          <button
            type="button"
            onClick={handleClose}
            className="btn btn-sm btn-ghost btn-circle"
          >
            <X size={18} />
          </button>
        </div>

        <div className="p-6">
          {step.image && (
            <div className="mb-4 flex justify-center">
              {step.image}
            </div>
          )}
          {step.content && (
            <div className="mb-4">
              {step.content}
            </div>
          )}
          <h3 className="text-xl font-bold mb-2">{t(step.titleKey)}</h3>
          <p className="text-base-content/80 leading-relaxed">{t(step.descriptionKey)}</p>
        </div>

        <div className="flex items-center justify-between p-4 border-t border-base-300">
          <button
            type="button"
            onClick={handlePrev}
            disabled={currentStep === 0}
            className="btn btn-sm btn-ghost gap-1"
          >
            <ChevronLeft size={16} />
            {t("tutorial.previous")}
          </button>

          <div className="flex gap-1.5">
            {steps.map((_, i) => (
              <div
                key={i}
                className={`w-2 h-2 rounded-full transition-colors ${
                  i === currentStep ? "bg-primary" : "bg-base-300"
                }`}
              />
            ))}
          </div>

          <button
            type="button"
            onClick={currentStep === steps.length - 1 ? handleClose : handleNext}
            className="btn btn-sm btn-primary gap-1"
          >
            {currentStep === steps.length - 1 ? t("tutorial.finish") : t("tutorial.next")}
            {currentStep < steps.length - 1 && <ChevronRight size={16} />}
          </button>
        </div>
      </div>
    </div>
  );
}
