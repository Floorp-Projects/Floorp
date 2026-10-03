import styles from "@/components/common/settings-sections.module.css";
import { Modal } from "../../../../../../libs/ui/modal.tsx";
import type { AppDialog } from "../types.ts";
import { Button } from "../../../../../../libs/ui/button.tsx";
import { Select } from "../../../../../../libs/ui/dropdown.tsx";
import { Input } from "../../../../../../libs/ui/input.tsx";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { type ChangeEvent, useEffect, useState } from "react";
import type { InstalledApp, TProgressiveWebAppObject } from "@/types/pref.ts";
import {
  type Container,
  getContainers,
  getInstalledApps,
  renamePwaApp,
  setSsbContainer,
  uninstallPwaApp,
} from "../dataManager.ts";
import { LayoutGrid } from "lucide-react";

export function InstalledApps() {
  const { t } = useTranslation();
  const [installedApps, setInstalledApps] = useState<TProgressiveWebAppObject>(
    {},
  );
  const [containers, setContainers] = useState<Container[]>([]);
  const [selectedApp, setSelectedApp] = useState<InstalledApp | null>(null);
  const [newName, setNewName] = useState("");
  const [selectedContainerId, setSelectedContainerId] = useState<number>(0);
  const [error, setError] = useState<string>("");
  // Container features are gated behind the pwa_container_support experiment.
  // When NRGetContainers is not exported, the experiment is disabled.
  const containerExperimentEnabled =
    typeof globalThis.NRGetContainers === "function";
  const [activeDialog, setActiveDialog] = useState<AppDialog>(null);

  const fetchApps = async () => {
    try {
      const apps = await getInstalledApps();
      setInstalledApps(apps);
      setError("");
    } catch (e) {
      setError(t("progressiveWebApp.errorFetchingApps"));
      console.error("Error fetching apps:", e);
    }
  };

  const fetchContainers = async () => {
    try {
      const list = await getContainers();
      setContainers(list);
    } catch (e) {
      console.error("Error fetching containers:", e);
    }
  };

  useEffect(() => {
    fetchApps();
    fetchContainers();
    document.documentElement.addEventListener("focus", fetchApps);
    return () => {
      document.documentElement.removeEventListener("focus", fetchApps);
    };
  }, []);

  const getContainerName = (userContextId?: number): string => {
    if (!userContextId || userContextId === 0) {
      return t("progressiveWebApp.noContainer");
    }
    const container = containers.find((c) => c.userContextId === userContextId);
    return container?.name ?? t("progressiveWebApp.containerDeleted");
  };

  const isContainerDeleted = (userContextId?: number): boolean => {
    if (!userContextId || userContextId === 0) return false;
    return !containers.some((c) => c.userContextId === userContextId);
  };

  const getContainerColor = (userContextId?: number): string | null => {
    if (!userContextId || userContextId === 0) return null;
    const container = containers.find((c) => c.userContextId === userContextId);
    if (!container) return null;
    return container.color || null;
  };

  const handleRename = (app: InstalledApp) => {
    setSelectedApp(app);
    setNewName(app.name);
    setError("");
    setActiveDialog("rename");
  };

  const handleUninstall = (app: InstalledApp) => {
    setSelectedApp(app);
    setError("");
    setActiveDialog("uninstall");
  };

  const handleContainer = (app: InstalledApp) => {
    setSelectedApp(app);
    setSelectedContainerId(app.userContextId ?? 0);
    setError("");
    setActiveDialog("container");
  };

  const executeRename = async () => {
    if (!selectedApp || !newName) return;

    try {
      await renamePwaApp(selectedApp.id, newName);
      setActiveDialog(null);
      setError("");
      setTimeout(() => {
        fetchApps();
      }, 1000);
    } catch (e) {
      setError(t("progressiveWebApp.errorRenaming"));
      console.error("Error renaming app:", e);
    }
  };

  const executeUninstall = async () => {
    if (!selectedApp) return;

    try {
      await uninstallPwaApp(selectedApp.id);
      setActiveDialog(null);
      setError("");
      setTimeout(() => {
        fetchApps();
      }, 1000);
    } catch (e) {
      setError(t("progressiveWebApp.errorUninstalling"));
      console.error("Error uninstalling app:", e);
    }
  };

  const executeSetContainer = async () => {
    if (!selectedApp) return;

    try {
      await setSsbContainer(selectedApp.id, selectedContainerId);
      setActiveDialog(null);
      setError("");
      setTimeout(() => {
        fetchApps();
      }, 1000);
    } catch (e) {
      setError(
        t(
          e instanceof Error && e.message === "native-context-fixed"
            ? "progressiveWebApp.nativeContainerFixed"
            : "progressiveWebApp.errorSettingContainer",
        ),
      );
      console.error("Error setting container:", e);
    }
  };

  const handleClose = () => {
    setError("");
    setActiveDialog(null);
  };

  return (
    <>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <LayoutGrid className="size-5" />
            {t("progressiveWebApp.installedApps")}
          </CardTitle>
        </CardHeader>
        <CardContent>
          {error && <p role="alert" className="text-error mb-4">{error}</p>}
          {Object.keys(installedApps).length === 0
            ? (
              <p className="text-base-content/70">
                {t("progressiveWebApp.noInstalledApps")}
              </p>
            )
            : (
              <div className="space-y-4">
                {(Object.values(installedApps) as InstalledApp[]).map((app) => {
                  const containerColor = containerExperimentEnabled
                    ? getContainerColor(app.userContextId)
                    : null;
                  return (
                    <div
                      key={app.id}
                      className={styles.appItem}
                      style={containerColor
                        ? {
                          borderColor: containerColor,
                          borderWidth: "2px",
                          backgroundColor: `${containerColor}10`,
                        }
                        : undefined}
                    >
                      <img
                        src={app.icon}
                        alt={app.name}
                        className="w-8 h-8 rounded"
                      />
                      <div className="flex-1 min-w-0">
                        <div className="flex flex-wrap items-center gap-2">
                          <p className="w-full font-medium truncate">
                            {app.name}
                          </p>
                          {containerExperimentEnabled &&
                            (app.userContextId ?? 0) > 0 && (
                            <span
                              className={`text-xs px-1.5 py-0.5 rounded ${
                                isContainerDeleted(app.userContextId)
                                  ? "bg-warning/20 text-warning"
                                  : "bg-base-200 text-base-content/70"
                              }`}
                            >
                              {getContainerName(app.userContextId)}
                              {isContainerDeleted(app.userContextId) && (
                                <Button
                                  type="button"
                                  variant="secondary"
                                  className="ml-1 text-xs underline"
                                  onClick={async () => {
                                    await setSsbContainer(app.id, 0);
                                    fetchApps();
                                  }}
                                >
                                  {t("progressiveWebApp.resetContainer")}
                                </Button>
                              )}
                            </span>
                          )}
                        </div>
                        <p className="text-sm text-base-content/70 truncate">
                          {app.start_url}
                        </p>
                      </div>
                      <div className={styles.actions}>
                        {containerExperimentEnabled && (
                          <Button
                            type="button"
                            variant="secondary"
                            onClick={() => handleContainer(app)}
                          >
                            {t("progressiveWebApp.container")}
                          </Button>
                        )}
                        <Button
                          type="button"
                          variant="secondary"
                          onClick={() => handleRename(app)}
                        >
                          {t("progressiveWebApp.renameApp")}
                        </Button>
                        <Button
                          type="button"
                          variant="danger"
                          onClick={() => handleUninstall(app)}
                        >
                          {t("progressiveWebApp.uninstallApp")}
                        </Button>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}
        </CardContent>
      </Card>

      {activeDialog === "container" && (
        <Modal
          title={t("progressiveWebApp.setContainer")}
          onClose={handleClose}
          closeLabel={t("progressiveWebApp.cancel")}
        >
          {error && <p role="alert" className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.containerDescription")}
          </p>
          <Select
            aria-label={t("progressiveWebApp.container")}
            className="w-full mb-4"
            value={selectedContainerId}
            onChange={(e) => setSelectedContainerId(Number(e.target.value))}
          >
            <option value={0}>
              {t("progressiveWebApp.noContainer")}
            </option>
            {containers.map((c) => (
              <option key={c.userContextId} value={c.userContextId}>
                {c.name}
              </option>
            ))}
          </Select>
          <div className="flex flex-wrap justify-end gap-3 mt-6">
            <Button
              type="button"
              variant="secondary"
              onClick={handleClose}
            >
              {t("progressiveWebApp.cancel")}
            </Button>
            <Button
              type="button"
              variant="primary"
              onClick={executeSetContainer}
            >
              {t("progressiveWebApp.save")}
            </Button>
          </div>
        </Modal>
      )}

      {activeDialog === "rename" && (
        <Modal
          title={t("progressiveWebApp.renameApp")}
          onClose={handleClose}
          closeLabel={t("progressiveWebApp.cancel")}
        >
          {error && <p role="alert" className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.enterNewName")}
          </p>
          <Input
            aria-label={t("progressiveWebApp.enterNewName")}
            type="text"
            value={newName}
            onChange={(e: ChangeEvent<HTMLInputElement>) =>
              setNewName(e.target.value)}
            className="w-full mb-4"
          />
          <div className="flex flex-wrap justify-end gap-3 mt-6">
            <Button
              type="button"
              variant="secondary"
              onClick={handleClose}
            >
              {t("progressiveWebApp.cancel")}
            </Button>
            <Button
              type="button"
              variant="primary"
              onClick={executeRename}
              disabled={!newName.trim() || newName === selectedApp?.name}
            >
              {t("progressiveWebApp.rename")}
            </Button>
          </div>
        </Modal>
      )}

      {activeDialog === "uninstall" && (
        <Modal
          title={t("progressiveWebApp.uninstallConfirmation")}
          onClose={handleClose}
          closeLabel={t("progressiveWebApp.cancel")}
        >
          {error && <p role="alert" className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.uninstallWarning", {
              name: selectedApp?.name,
            })}
          </p>
          <div className="flex flex-wrap justify-end gap-3 mt-6">
            <Button
              type="button"
              variant="secondary"
              onClick={handleClose}
            >
              {t("progressiveWebApp.cancel")}
            </Button>
            <Button
              type="button"
              variant="danger"
              onClick={executeUninstall}
            >
              {t("progressiveWebApp.uninstall")}
            </Button>
          </div>
        </Modal>
      )}
    </>
  );
}
