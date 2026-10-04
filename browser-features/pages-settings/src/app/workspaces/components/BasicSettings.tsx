import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { InfoTip } from "@/components/common/infotip.tsx";
import { ExternalLink } from "lucide-react";
import { RestartModal } from "@/components/common/restart-modal.tsx";
import { useState } from "react";
import { Button } from "@/components/common/button.tsx";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import { initializeWorkspaces } from "../dataManager.ts";
import type { WorkspacesFormData } from "@/types/pref.ts";

import styles from "../workspace.module.css";

export function BasicSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<WorkspacesFormData>();
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
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle>{t("workspaces.basicSettings")}</CardTitle>
        </CardHeader>
        <CardContent>
          <div className={styles.row}>
            <div className="space-y-1">
              <div className="flex items-center gap-2">
                <label htmlFor="enable-workspaces" className="font-semibold">
                  {t("workspaces.enableWorkspaces")}
                </label>
                <InfoTip
                  description={t("workspaces.enableWorkspacesDescription")}
                />
              </div>
              <p
                id="workspace-restart-hint"
                className="text-sm text-muted-foreground"
              >
                {t("workspaces.needRestartDescriptionForEnableAndDisable")}
              </p>
            </div>
            <Switch
              id="enable-workspaces"
              aria-describedby="workspace-restart-hint"
              checked={!!getValues("enabled")}
              onChange={(e) => {
                setValue("enabled", e.target.checked);
                setShowRestartModal(true);
              }}
            />
          </div>
        </CardContent>
      </Card>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle>{t("workspaceRefresh.behavior")}</CardTitle>
          <a
            href="https://docs.floorp.app/docs/features/workspaces"
            target="_blank"
            rel="noopener noreferrer"
            className={styles.help}
          >
            {t("workspaceRefresh.help")}
            <ExternalLink size={16} aria-hidden="true" />
          </a>
        </CardHeader>
        <CardContent className="space-y-3">
          <div className={styles.row}>
            <label htmlFor="close-popup" className="flex flex-col gap-1.5">
              <span>{t("workspaces.closePopupWhenSelectingWorkspace")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.closePopupWhenSelectingWorkspaceDescription")}
              </span>
            </label>
            <Switch
              id="close-popup"
              checked={!!getValues("closePopupAfterClick")}
              onChange={(e) =>
                setValue("closePopupAfterClick", e.target.checked)}
            />
          </div>

          <div className={styles.row}>
            <label htmlFor="show-name">
              {t("workspaces.showWorkspaceNameOnToolbar")}
            </label>
            <Switch
              id="show-name"
              checked={!!getValues("showWorkspaceNameOnToolbar")}
              onChange={(e) =>
                setValue("showWorkspaceNameOnToolbar", e.target.checked)}
            />
          </div>

          <div className={styles.row}>
            <label htmlFor="exit-on-last-tab-close">
              {t("workspaces.exitOnLastTabClose")}
            </label>
            <Switch
              id="exit-on-last-tab-close"
              checked={!!getValues("exitOnLastTabClose")}
              onChange={(e) => setValue("exitOnLastTabClose", e.target.checked)}
            />
          </div>

          <div className={styles.row}>
            <label htmlFor="manage-bms" className="flex flex-col gap-1.5">
              <span>{t("workspaces.manageOnBms")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.manageOnBmsDescription")}
              </span>
            </label>
            <Switch
              id="manage-bms"
              checked={!!getValues("manageOnBms")}
              onChange={(e) => setValue("manageOnBms", e.target.checked)}
            />
          </div>
        </CardContent>
      </Card>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle>{t("workspaceRefresh.reset")}</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="space-y-1">
            <p className="text-sm text-muted-foreground">
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
          <div className={styles.resetActions}>
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
        </CardContent>
      </Card>
    </>
  );
}
