import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { InfoTip } from "@nora/ui-components";
import { ExternalLink, Settings } from "lucide-react";
import { RestartModal } from "@/components/common/restart-modal.tsx";
import { useState } from "react";
import { Button } from "@nora/ui-components";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import { initializeWorkspaces } from "../dataManager.ts";

export function BasicSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();
  const [showRestartModal, setShowRestartModal] = useState(false);
  const [showInitializeModal, setShowInitializeModal] = useState(false);
  const [isInitializing, setIsInitializing] = useState(false);
  const [initializeStatus, setInitializeStatus] = useState<
    "idle" | "success" | "error"
  >("idle");

  const handleConfirmInitialize = async () => {
    setIsInitializing(true);
    setInitializeStatus("idle");
    try {
      const success = await initializeWorkspaces("settings-page");
      setInitializeStatus(success ? "success" : "error");
    } catch (error) {
      console.error("Failed to initialize workspaces:", error);
      setInitializeStatus("error");
    } finally {
      setIsInitializing(false);
    }
  };

  return (
    <>
      {showRestartModal
        ? (
          <RestartModal
            onClose={() => setShowRestartModal(false)}
            label={t("workspaces.needRestartDescriptionForEnableAndDisable")}
          />
        )
        : null}
      {showInitializeModal
        ? (
          <ConfirmModal
            isOpen={showInitializeModal}
            onClose={() => setShowInitializeModal(false)}
            onConfirm={handleConfirmInitialize}
            title={t("workspaces.initializeConfirmTitle")}
            confirmText={t("workspaces.initializeConfirmAction")}
            cancelText={t("workspaces.initializeConfirmCancel")}
            confirmVariant="danger"
          >
            <p>{t("workspaces.initializeConfirmDescription")}</p>
          </ConfirmModal>
        )
        : null}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Settings className="size-5" />
            {t("workspaces.basicSettings")}
          </CardTitle>
          <CardDescription>
            <a
              href="https://docs.floorp.app/docs/features/how-to-use-workspaces"
              className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
            >
              {t("workspaces.howToUseAndCustomize")}
              <ExternalLink className="size-4" />
            </a>
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-3">
          <div>
            <div className="mb-2 inline-flex items-center gap-2">
              <h3 className="text-base font-medium">
                {t("workspaces.enableOrDisable")}
              </h3>
              <InfoTip
                description={t("workspaces.enableWorkspacesDescription")}
              />
            </div>
            <div className="space-y-1">
              <div className="flex items-center justify-between gap-2">
                <div className="space-y-1">
                  <label htmlFor="enable-workspaces">
                    {t("workspaces.enableWorkspaces")}
                  </label>
                </div>
                <Switch
                  id="enable-workspaces"
                  checked={getValues("enabled")}
                  onChange={(e) => {
                    setValue("enabled", e.target.checked);
                    setShowRestartModal(true);
                  }}
                />
              </div>
              <div className="text-sm text-base-content/70">
                {t("workspaces.needRestartDescriptionForEnableAndDisable")}
              </div>
            </div>
          </div>

          <h3 className="text-base font-medium">
            {t("workspaces.otherSettings")}
          </h3>
          <div className="space-y-3">
            <div className="flex items-center justify-between gap-2">
              <label htmlFor="close-popup" className="flex flex-col gap-1.5">
                <span>{t("workspaces.closePopupWhenSelectingWorkspace")}</span>
                <span className="font-normal text-sm text-base-content/70">
                  {t("workspaces.closePopupWhenSelectingWorkspaceDescription")}
                </span>
              </label>
              <Switch
                id="close-popup"
                checked={getValues("closePopupAfterClick")}
                onChange={(e) =>
                  setValue("closePopupAfterClick", e.target.checked)}
              />
            </div>

            <div className="flex items-center justify-between gap-2">
              <label htmlFor="show-name">
                {t("workspaces.showWorkspaceNameOnToolbar")}
              </label>
              <Switch
                id="show-name"
                checked={getValues("showWorkspaceNameOnToolbar")}
                onChange={(e) =>
                  setValue("showWorkspaceNameOnToolbar", e.target.checked)}
              />
            </div>

            <div className="flex items-center justify-between gap-2">
              <label htmlFor="exit-on-last-tab-close">
                {t("workspaces.exitOnLastTabClose")}
              </label>
              <Switch
                id="exit-on-last-tab-close"
                checked={getValues("exitOnLastTabClose")}
                onChange={(e) =>
                  setValue("exitOnLastTabClose", e.target.checked)}
              />
            </div>

            <div className="flex items-center justify-between gap-2">
              <label htmlFor="manage-bms" className="flex flex-col gap-1.5">
                <span>{t("workspaces.manageOnBms")}</span>
                <span className="font-normal text-sm text-base-content/70">
                  {t("workspaces.manageOnBmsDescription")}
                </span>
              </label>
              <Switch
                id="manage-bms"
                checked={getValues("manageOnBms")}
                onChange={(e) => setValue("manageOnBms", e.target.checked)}
              />
            </div>
          </div>

          <h3 className="text-base font-medium mt-4">
            {t("workspaces.dangerZone")}
          </h3>

          <div className="flex flex-col gap-2 rounded-lg border border-red-200 dark:border-red-800 p-4 bg-red-50 dark:bg-red-950/20">
            <div className="space-y-1">
              <p className="text-base font-medium text-red-700 dark:text-red-300">
                {t("workspaces.initializeTitle")}
              </p>
              <p className="text-sm text-red-600 dark:text-red-400">
                {t("workspaces.initializeDescription")}
              </p>
              {initializeStatus === "success"
                ? (
                  <p className="text-sm text-green-600 dark:text-green-400">
                    {t("workspaces.initializeSuccess")}
                  </p>
                )
                : initializeStatus === "error"
                ? (
                  <p className="text-sm text-red-600 dark:text-red-400">
                    {t("workspaces.initializeFailure")}
                  </p>
                )
                : null}
            </div>
            <div className="flex justify-end">
              <Button
                variant="danger"
                disabled={isInitializing}
                onClick={() => {
                  setInitializeStatus("idle");
                  setShowInitializeModal(true);
                }}
              >
                {t("workspaces.initializeAction")}
              </Button>
            </div>
          </div>
        </CardContent>
      </Card>
    </>
  );
}
