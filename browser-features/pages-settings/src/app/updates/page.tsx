import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { DropDown } from "@/components/common/dropdown.tsx";
import { Button, ButtonProps } from "@/components/common/button.tsx";
import { useCallback, useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";
import { rpc } from "@/lib/rpc/rpc.ts";
import { experimentsRpc } from "@/lib/rpc/experiments.ts";
import { CheckCircle2, RefreshCw, Trash2, X, XCircle, Zap } from "lucide-react";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";

const EXPERIMENTS_POLICY_PREF = "floorp.experiments.participationPolicy";

interface ActiveExperiment {
  id: string;
  name?: string;
  description?: string;
  variantId: string;
  variantName: string;
  assignedAt: string | null;
  experimentData: Record<string, unknown>;
  disabled: boolean;
}

export default function Page() {
  const { t } = useTranslation();
  const [participationPolicy, setParticipationPolicy] = useState<string>(
    "default",
  );
  const [activeExperiments, setActiveExperiments] = useState<
    ActiveExperiment[]
  >([]);
  const [loading, setLoading] = useState(true);
  const [experimentsLoading, setExperimentsLoading] = useState(true);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);
  const [modalState, setModalState] = useState<
    {
      isOpen: boolean;
      title: string;
      description: React.ReactNode;
      onConfirm: () => void;
      confirmText?: string;
      confirmVariant?: ButtonProps["variant"];
    } | null
  >(null);

  const loadExperiments = useCallback(async () => {
    setExperimentsLoading(true);
    try {
      const experiments = await experimentsRpc.getActiveExperiments();
      setActiveExperiments(experiments || []);
      setLastUpdated(new Date());
    } catch (e) {
      console.error("Failed to load active experiments:", e);
    } finally {
      setExperimentsLoading(false);
    }
  }, []);

  useEffect(() => {
    async function loadPreferences() {
      try {
        const policy = await rpc.getStringPref(EXPERIMENTS_POLICY_PREF);
        setParticipationPolicy(policy || "default");
      } catch (e) {
        console.error("Failed to load experiments participation policy:", e);
      } finally {
        setLoading(false);
      }
    }
    loadPreferences();
    loadExperiments();
  }, [loadExperiments]);

  const handlePolicyChange = async (
    e: React.ChangeEvent<HTMLSelectElement>,
  ) => {
    const value = e.target.value;
    try {
      await rpc.setStringPref(EXPERIMENTS_POLICY_PREF, value);
      setParticipationPolicy(value);
      await experimentsRpc.reinitializeExperiments();
      await loadExperiments();
    } catch (error) {
      console.error("Failed to save experiments participation policy:", error);
    }
  };

  const openConfirmationModal = (
    title: string,
    description: React.ReactNode,
    onConfirm: () => void,
    confirmText?: string,
    confirmVariant?: ButtonProps["variant"],
  ) => {
    setModalState({
      isOpen: true,
      title,
      description,
      onConfirm,
      confirmText,
      confirmVariant,
    });
  };

  const handleCloseModal = () => {
    setModalState(null);
  };

  const handleDisableExperiment = (experimentId: string) => {
    openConfirmationModal(
      t("updates.activeTests.disable"),
      t("updates.activeTests.confirmDisable"),
      async () => {
        try {
          const result = await experimentsRpc.disableExperiment(experimentId);
          if (result.success) {
            await loadExperiments();
          } else {
            console.error("Failed to disable experiment:", result.error);
          }
        } catch (error) {
          console.error("Failed to disable experiment:", error);
        }
      },
      t("updates.activeTests.disable"),
      "secondary",
    );
  };

  const handleEnableExperiment = (experimentId: string) => {
    openConfirmationModal(
      t("updates.activeTests.enable"),
      t("updates.activeTests.confirmEnable"),
      async () => {
        try {
          const result = await experimentsRpc.enableExperiment(experimentId);
          if (result.success) {
            await loadExperiments();
          } else {
            console.error("Failed to enable experiment:", result.error);
          }
        } catch (error) {
          console.error("Failed to enable experiment:", error);
        }
      },
      t("updates.activeTests.enable"),
      "primary",
    );
  };

  const handleClearCache = () => {
    openConfirmationModal(
      t("updates.experiments.clearCache"),
      t("updates.experiments.confirmClearCache"),
      async () => {
        try {
          const result = await experimentsRpc.clearExperimentCache();
          if (result.success) {
            await loadExperiments();
          } else {
            console.error("Failed to clear experiment cache:", result.error);
          }
        } catch (error) {
          console.error("Failed to clear experiment cache:", error);
        }
      },
      t("updates.experiments.clearCache"),
      "danger",
    );
  };

  const policyOptions = useMemo(() => [
    {
      value: "default",
      label: t("updates.experiments.policy.default"),
      icon: <CheckCircle2 className="h-4 w-4 text-primary" />,
    },
    {
      value: "always",
      label: t("updates.experiments.policy.always"),
      icon: <Zap className="h-4 w-4 text-warning" />,
    },
    {
      value: "never",
      label: t("updates.experiments.policy.never"),
      icon: <XCircle className="h-4 w-4 text-base-content/50" />,
    },
  ], [t]);

  if (loading) {
    return <div className="p-6">{t("common.loading")}</div>;
  }

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">{t("updates.title")}</h1>
        <p className="text-sm mb-8">{t("updates.description")}</p>
      </div>

      <div className="flex flex-col gap-8 pl-6">
        <Card>
          <CardHeader>
            <CardTitle>{t("updates.experiments.title")}</CardTitle>
            <CardDescription>
              {t("updates.experiments.description")}
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-2">
              <label className="label">
                <span className="label-text">
                  {t("updates.experiments.policyLabel")}
                </span>
              </label>
              <DropDown
                value={participationPolicy}
                onChange={handlePolicyChange}
                options={policyOptions}
              />
              <p className="text-sm text-base-content/70 mt-2">
                {t(`updates.experiments.policyHelp.${participationPolicy}`)}
              </p>
            </div>
          </CardContent>
          <CardFooter className="flex-col items-start gap-4 pt-4 border-t border-base-content/10">
            <h3 className="text-sm font-medium">
              {t("updates.experiments.troubleshooting")}
            </h3>
            <p className="text-sm text-base-content/70">
              {t("updates.experiments.clearCacheDescription")}
            </p>
            <Button onClick={handleClearCache} variant="danger" size="sm">
              <Trash2 className="h-4 w-4 mr-2" />
              {t("updates.experiments.clearCache")}
            </Button>
          </CardFooter>
        </Card>

        <Card>
          <CardHeader>
            <div className="flex flex-row items-start justify-between gap-4">
              <div className="flex-1">
                <CardTitle>{t("updates.activeTests.title")}</CardTitle>
                <CardDescription>
                  {t("updates.activeTests.description")}
                  {lastUpdated &&
                    ` (${
                      t("updates.activeTests.lastUpdated")
                    } ${lastUpdated.toLocaleTimeString()})`}
                </CardDescription>
              </div>
              <Button
                onClick={loadExperiments}
                variant="primary"
                size="sm"
                disabled={experimentsLoading}
                className="shrink-0 h-8"
              >
                <RefreshCw
                  className={`h-4 w-4 mr-1 ${
                    experimentsLoading ? "animate-spin" : ""
                  }`}
                />
                {t("updates.activeTests.refresh")}
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            {experimentsLoading
              ? (
                <div className="text-center py-8 text-base-content/70">
                  {t("common.loading")}
                </div>
              )
              : activeExperiments.length === 0
              ? (
                <div className="text-center py-8 text-base-content/70">
                  {t("updates.activeTests.noTests")}
                </div>
              )
              : (
                <div className="space-y-3">
                  {activeExperiments.map((experiment) => (
                    <div
                      key={experiment.id}
                      className={`flex items-start justify-between p-4 rounded-lg border transition-colors ${
                        experiment.disabled
                          ? "opacity-50 bg-base-content/5 border-base-content/10"
                          : "border-base-content/20 hover:border-base-content/40"
                      }`}
                    >
                      <div className="flex-1 pr-4">
                        <div className="flex items-center gap-2">
                          <h3 className="font-medium">
                            {experiment.name || experiment.id}
                          </h3>
                          <span className="text-xs px-2 py-1 rounded-full bg-primary/10 text-primary">
                            {experiment.variantName}
                          </span>
                          {experiment.disabled && (
                            <span className="text-xs px-2 py-1 rounded-full bg-base-content/10 text-base-content/70">
                              {t("updates.activeTests.disabledLabel")}
                            </span>
                          )}
                        </div>
                        {experiment.description && (
                          <p className="text-sm text-base-content/70 mt-1">
                            {experiment.description}
                          </p>
                        )}
                        <div className="text-xs text-base-content/50 mt-2 space-x-4">
                          <span>ID: {experiment.id}</span>
                          {experiment.assignedAt && (
                            <span>
                              {t("updates.activeTests.assignedAt", {
                                date: new Date(
                                  experiment.assignedAt,
                                ).toLocaleDateString(),
                              })}
                            </span>
                          )}
                        </div>
                      </div>
                      <Button
                        onClick={() =>
                          experiment.disabled
                            ? handleEnableExperiment(experiment.id)
                            : handleDisableExperiment(experiment.id)}
                        variant="secondary"
                        size="sm"
                        className="shrink-0 h-8 mt-1"
                      >
                        <X className="h-4 w-4 mr-1" />
                        {experiment.disabled
                          ? t("updates.activeTests.enable")
                          : t("updates.activeTests.disable")}
                      </Button>
                    </div>
                  ))}
                </div>
              )}
          </CardContent>
        </Card>
      </div>

      {modalState && (
        <ConfirmModal
          isOpen={modalState.isOpen}
          onClose={handleCloseModal}
          onConfirm={modalState.onConfirm}
          title={modalState.title}
          confirmText={modalState.confirmText}
          confirmVariant={modalState.confirmVariant}
        >
          {modalState.description}
        </ConfirmModal>
      )}
    </div>
  );
}
