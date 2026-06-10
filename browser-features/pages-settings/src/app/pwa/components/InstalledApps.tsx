import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { type ChangeEvent, useEffect, useRef, useState } from "react";
import type { InstalledApp, TProgressiveWebAppObject } from "@/types/pref.ts";
import {
  getInstalledApps,
  renamePwaApp,
  uninstallPwaApp,
  getContainers,
  setSsbContainer,
  type Container,
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
  const renameDialogRef = useRef<HTMLDialogElement>(null) as React.RefObject<
    HTMLDialogElement
  >;
  const uninstallDialogRef = useRef<HTMLDialogElement>(null) as React.RefObject<
    HTMLDialogElement
  >;
  const containerDialogRef = useRef<HTMLDialogElement>(null) as React.RefObject<
    HTMLDialogElement
  >;

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

  const CONTAINER_COLOR_MAP: Record<string, string> = {
    blue: "#37adff",
    turquoise: "#00c79a",
    green: "#51cd00",
    yellow: "#ffcb00",
    orange: "#ff9f00",
    red: "#ff613d",
    pink: "#ff4bda",
    purple: "#af51f5",
  };

  const getContainerColor = (userContextId?: number): string | null => {
    if (!userContextId || userContextId === 0) return null;
    const container = containers.find((c) => c.userContextId === userContextId);
    if (!container) return null;
    return CONTAINER_COLOR_MAP[container.color] ?? "#37adff";
  };

  const handleRename = (app: InstalledApp) => {
    setSelectedApp(app);
    setNewName(app.name);
    setError("");
    renameDialogRef.current?.showModal();
  };

  const handleUninstall = (app: InstalledApp) => {
    setSelectedApp(app);
    setError("");
    uninstallDialogRef.current?.showModal();
  };

  const handleContainer = (app: InstalledApp) => {
    setSelectedApp(app);
    setSelectedContainerId(app.userContextId ?? 0);
    setError("");
    containerDialogRef.current?.showModal();
  };

  const executeRename = async () => {
    if (!selectedApp || !newName) return;

    try {
      await renamePwaApp(selectedApp.id, newName);
      renameDialogRef.current?.close();
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
      uninstallDialogRef.current?.close();
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
      containerDialogRef.current?.close();
      setError("");
      setTimeout(() => {
        fetchApps();
      }, 1000);
    } catch (e) {
      setError(t("progressiveWebApp.errorSettingContainer"));
      console.error("Error setting container:", e);
    }
  };

  const handleClose = (dialogRef: React.RefObject<HTMLDialogElement>) => {
    setError("");
    dialogRef.current?.close();
  };

  return (
    <>
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <LayoutGrid className="size-5" />
            {t("progressiveWebApp.installedApps")}
          </CardTitle>
        </CardHeader>
        <CardContent>
          {error && <p className="text-error mb-4">{error}</p>}
          {Object.keys(installedApps).length === 0
            ? (
              <p className="text-base-content/70">
                {t("progressiveWebApp.noInstalledApps")}
              </p>
            )
            : (
              <div className="space-y-4">
                {(Object.values(installedApps) as InstalledApp[]).map((app) => {
                  const containerColor = getContainerColor(app.userContextId);
                  return (
                  <div
                    key={app.id}
                    className="flex items-center p-3 border rounded-lg"
                    style={containerColor ? {
                      borderColor: containerColor,
                      borderWidth: "2px",
                      backgroundColor: `${containerColor}10`,
                    } : undefined}
                  >
                    <img
                      src={app.icon}
                      alt={app.name}
                      className="w-8 h-8 rounded mr-3"
                    />
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center gap-2">
                        <p className="font-medium truncate">{app.name}</p>
                        {app.userContextId && app.userContextId > 0 && (
                          <span
                            className={`text-xs px-1.5 py-0.5 rounded ${
                              isContainerDeleted(app.userContextId)
                                ? "bg-warning/20 text-warning"
                                : "bg-base-200 text-base-content/70"
                            }`}
                          >
                            {getContainerName(app.userContextId)}
                            {isContainerDeleted(app.userContextId) && (
                              <button
                                type="button"
                                className="ml-1 text-xs underline"
                                onClick={async () => {
                                  await setSsbContainer(app.id, 0);
                                  fetchApps();
                                }}
                              >
                                {t("progressiveWebApp.resetContainer")}
                              </button>
                            )}
                          </span>
                        )}
                      </div>
                      <p className="text-sm text-base-content/70 truncate">
                        {app.start_url}
                      </p>
                    </div>
                    <div className="flex gap-2 ml-4">
                      <button
                        type="button"
                        className="btn btn-sm"
                        onClick={() => handleContainer(app)}
                      >
                        {t("progressiveWebApp.container")}
                      </button>
                      <button
                        type="button"
                        className="btn btn-sm"
                        onClick={() => handleRename(app)}
                      >
                        {t("progressiveWebApp.renameApp")}
                      </button>
                      <button
                        type="button"
                        className="btn btn-sm btn-error"
                        onClick={() => handleUninstall(app)}
                      >
                        {t("progressiveWebApp.uninstallApp")}
                      </button>
                    </div>
                  </div>
                  );
                })}
              </div>
            )}
        </CardContent>
      </Card>

      <dialog
        ref={containerDialogRef}
        className="modal modal-bottom sm:modal-middle"
        onClick={(e) => {
          if (e.target === containerDialogRef.current) {
            handleClose(containerDialogRef);
          }
        }}
      >
        <div className="modal-box">
          <h3 className="font-bold text-lg">
            {t("progressiveWebApp.setContainer")}
          </h3>
          {error && <p className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.containerDescription")}
          </p>
          <select
            className="select select-bordered w-full mb-4"
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
          </select>
          <div className="modal-action">
            <button
              type="button"
              className="btn"
              onClick={() => handleClose(containerDialogRef)}
            >
              {t("progressiveWebApp.cancel")}
            </button>
            <button
              type="button"
              className="btn btn-primary"
              onClick={executeSetContainer}
            >
              {t("progressiveWebApp.save")}
            </button>
          </div>
        </div>
        <form method="dialog" className="modal-backdrop">
          <button type="submit" onClick={() => handleClose(containerDialogRef)}>close</button>
        </form>
      </dialog>

      <dialog
        ref={renameDialogRef}
        className="modal modal-bottom sm:modal-middle"
        onClick={(e) => {
          if (e.target === renameDialogRef.current) {
            handleClose(renameDialogRef);
          }
        }}
      >
        <div className="modal-box">
          <h3 className="font-bold text-lg">
            {t("progressiveWebApp.renameApp")}
          </h3>
          {error && <p className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.enterNewName")}
          </p>
          <input
            type="text"
            value={newName}
            onChange={(e: ChangeEvent<HTMLInputElement>) =>
              setNewName(e.target.value)}
            className="input input-bordered w-full mb-4"
          />
          <div className="modal-action">
            <button
              type="button"
              className="btn"
              onClick={() => handleClose(renameDialogRef)}
            >
              {t("progressiveWebApp.cancel")}
            </button>
            <button
              type="button"
              className="btn btn-primary"
              onClick={executeRename}
              disabled={!newName.trim() || newName === selectedApp?.name}
            >
              {t("progressiveWebApp.rename")}
            </button>
          </div>
        </div>
        <form method="dialog" className="modal-backdrop">
          <button type="submit" onClick={() => handleClose(renameDialogRef)}>close</button>
        </form>
      </dialog>

      <dialog
        ref={uninstallDialogRef}
        className="modal modal-bottom sm:modal-middle"
        onClick={(e) => {
          if (e.target === uninstallDialogRef.current) {
            handleClose(uninstallDialogRef);
          }
        }}
      >
        <div className="modal-box">
          <h3 className="font-bold text-lg">
            {t("progressiveWebApp.uninstallConfirmation")}
          </h3>
          {error && <p className="text-error mb-4">{error}</p>}
          <p className="py-4 text-base-content/70">
            {t("progressiveWebApp.uninstallWarning", {
              name: selectedApp?.name,
            })}
          </p>
          <div className="modal-action">
            <button
              type="button"
              className="btn"
              onClick={() => handleClose(uninstallDialogRef)}
            >
              {t("progressiveWebApp.cancel")}
            </button>
            <button
              type="button"
              className="btn btn-error"
              onClick={executeUninstall}
            >
              {t("progressiveWebApp.uninstall")}
            </button>
          </div>
        </div>
        <form method="dialog" className="modal-backdrop">
          <button type="submit" onClick={() => handleClose(uninstallDialogRef)}>close</button>
        </form>
      </dialog>
    </>
  );
}
