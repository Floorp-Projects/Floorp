import { Select } from "../../../../../../libs/ui/dropdown.tsx";
import { Input } from "../../../../../../libs/ui/input.tsx";
import { Button } from "../../../../../../libs/ui/button.tsx";
import { Modal } from "../../../../../../libs/ui/modal.tsx";
import type React from "react";
import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import type { Panel } from "../../../../../chrome/common/panel-sidebar/utils/type.ts";
import {
  getContainers,
  getExtensionPanels,
  getStaticPanelDisplayName,
  getStaticPanels,
} from "../dataManager.ts";

import type {
  Container,
  ExtensionPanel,
  PanelEditModalProps,
  StaticPanel,
} from "../types.ts";

export const PanelEditModal: React.FC<PanelEditModalProps> = ({
  panel,
  onSave,
  onClose,
}) => {
  const { t } = useTranslation();
  const [editedPanel, setEditedPanel] = useState<Panel>({ ...panel });
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [containers, setContainers] = useState<Container[]>([]);
  const [staticPanels, setStaticPanels] = useState<StaticPanel[]>([]);
  const [extensionPanels, setExtensionPanels] = useState<ExtensionPanel[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  const fetchPanelData = useCallback(async () => {
    setIsLoading(true);
    try {
      const [containersData, staticPanelsData, extensionPanelsData] =
        await Promise.all([
          getContainers(),
          getStaticPanels(),
          getExtensionPanels(),
        ]);

      if (containersData && Array.isArray(containersData)) {
        setContainers(containersData);
      } else {
        console.error("Invalid containers data", containersData);
        setContainers([]);
      }

      if (staticPanelsData && Array.isArray(staticPanelsData)) {
        setStaticPanels(staticPanelsData);
      } else {
        setStaticPanels([]);
      }

      if (extensionPanelsData && Array.isArray(extensionPanelsData)) {
        setExtensionPanels(extensionPanelsData);
      } else {
        setExtensionPanels([]);
      }
    } catch (error) {
      console.error("Failed to fetch panel data:", error);
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchPanelData();

    return () => {
    };
  }, [fetchPanelData]);

  const shouldShowWebFields = editedPanel.type === "web";
  const shouldShowStaticFields = editedPanel.type === "static";
  const shouldShowExtensionFields = editedPanel.type === "extension";

  const validateAndFormatUrl = (url: string): string => {
    if (!url) return "";

    if (url.startsWith("http://") || url.startsWith("https://")) {
      return url;
    }

    return `https://${url}`;
  };

  const handleChange = (
    e: React.ChangeEvent<HTMLInputElement | HTMLSelectElement>,
  ) => {
    const { name, value } = e.target;

    if (name === "type") {
      const newPanel = {
        ...editedPanel,
        [name]: value as "web" | "static" | "extension",
      };

      if (value === "web") {
        newPanel.extensionId = null;
      } else if (value === "extension") {
        newPanel.url = "";
        newPanel.userAgent = null;
        newPanel.userContextId = null;
      } else if (value === "static") {
        newPanel.extensionId = null;
        newPanel.url = "";
        newPanel.userAgent = null;
        newPanel.userContextId = null;
      }

      setEditedPanel(newPanel);
    } else if (name === "url") {
      if (editedPanel.type === "web") {
        const formattedUrl = validateAndFormatUrl(value);
        setEditedPanel({ ...editedPanel, [name]: formattedUrl });
      } else {
        setEditedPanel({ ...editedPanel, [name]: value });
      }
    } else if (name === "width") {
      setEditedPanel({ ...editedPanel, [name]: Number(value) });
    } else if (name === "zoomLevel") {
      setEditedPanel({ ...editedPanel, [name]: value ? Number(value) : null });
    } else if (name === "userContextId") {
      const numericValue = value ? Number(value) : null;
      setEditedPanel({ ...editedPanel, [name]: numericValue });
    } else if (name === "userAgent") {
      setEditedPanel({ ...editedPanel, [name]: value === "true" });
    } else if (name === "extensionId") {
      setEditedPanel({ ...editedPanel, [name]: value || null });
    } else {
      setEditedPanel({ ...editedPanel, [name]: value });
    }
  };

  const handleCheckboxChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, checked } = e.target;
    setEditedPanel({ ...editedPanel, [name]: checked });
  };

  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!editedPanel.type) {
      newErrors.type = t("panelSidebar.errors.typeRequired");
    }

    if (editedPanel.type === "web") {
      if (!editedPanel.url) {
        newErrors.url = t("panelSidebar.errors.urlRequired");
      } else {
        try {
          new URL(editedPanel.url);
        } catch (e) {
          console.error("Invalid URL:", e);
          newErrors.url = t("panelSidebar.errors.invalidUrl");
        }
      }
    }

    if (editedPanel.type === "static" && !editedPanel.url) {
      newErrors.url = t("panelSidebar.errors.staticPanelRequired");
    }

    if (editedPanel.type === "extension" && !editedPanel.extensionId) {
      newErrors.extensionId = t("panelSidebar.errors.extensionIdRequired");
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (validateForm()) {
      onSave(editedPanel);
    }
  };

  if (isLoading) {
    return (
      <Modal
        title={t("panelSidebar.editPanel")}
        onClose={onClose}
        closeLabel={t("panelSidebar.cancel")}
      >
        <p role="status">{t("ui.loading")}</p>
      </Modal>
    );
  }

  return (
    <Modal
      wide
      title={editedPanel.id.startsWith("panel-")
        ? t("panelSidebar.addPanel")
        : t("panelSidebar.editPanel")}
      onClose={onClose}
      closeLabel={t("panelSidebar.cancel")}
    >
      <form onSubmit={handleSubmit} className="space-y-4">
        <div className="floorp-field">
          <label className="floorp-field-label" htmlFor="panel-type">
            <span className="floorp-field-text">{t("panelSidebar.panelType")}</span>
          </label>
          <Select
            name="type" id="panel-type" aria-label={t("panelSidebar.panelType")}
            value={editedPanel.type}
            onChange={handleChange}
            className="w-full"
          >
            <option value="web">{t("panelSidebar.type.web")}</option>
            <option value="static">{t("panelSidebar.type.static")}</option>
            <option value="extension">
              {t("panelSidebar.type.extension")}
            </option>
          </Select>
          {errors.type && (
            <label className="floorp-field-label">
              <span className="floorp-field-hint text-error">{errors.type}</span>
            </label>
          )}
        </div>

        {shouldShowWebFields && (
          <div className="floorp-field">
            <label className="floorp-field-label" htmlFor="panel-url">
              <span className="floorp-field-text">{t("panelSidebar.url")}</span>
            </label>
            <div className="relative">
              <Input
                type="text"
                name="url" id="panel-url" aria-label={t("panelSidebar.url")}
                value={editedPanel.url || ""}
                onChange={handleChange}
                placeholder="example.com"
                className="w-full pr-8"
              />
              <div className="absolute inset-y-0 right-0 flex items-center pr-3 pointer-events-none text-base-content/50">
                {editedPanel.url && (
                  <span className="text-xs">
                    {editedPanel.url.startsWith("https://") ? "🔒" : "⚠️"}
                  </span>
                )}
              </div>
            </div>
            {errors.url && (
              <label className="floorp-field-label">
                <span className="floorp-field-hint text-error">
                  {errors.url}
                </span>
              </label>
            )}
          </div>
        )}

        {shouldShowStaticFields && (
          <div className="floorp-field">
            <label className="floorp-field-label" htmlFor="panel-url">
              <span className="floorp-field-text">
                {t("panelSidebar.staticPanel")}
              </span>
            </label>
            <Select
              name="url" id="panel-url" aria-label={t("panelSidebar.url")}
              value={editedPanel.url || ""}
              onChange={handleChange}
              className="w-full"
            >
              <option value="">{t("panelSidebar.selectStaticPanel")}</option>
              {staticPanels.map((panel) => (
                <option key={panel.value} value={panel.value}>
                  {getStaticPanelDisplayName(panel.value, t)}
                </option>
              ))}
            </Select>
            {errors.url && (
              <label className="floorp-field-label">
                <span className="floorp-field-hint text-error">
                  {errors.url}
                </span>
              </label>
            )}
          </div>
        )}

        {shouldShowExtensionFields && (
          <div className="floorp-field">
            <label className="floorp-field-label" htmlFor="panel-extensionId">
              <span className="floorp-field-text">
                {t("panelSidebar.extensionPanel")}
              </span>
            </label>
            <Select
              name="extensionId" id="panel-extensionId" aria-label={t("panelSidebar.extensionPanel")}
              value={editedPanel.extensionId || ""}
              onChange={handleChange}
              className="w-full"
            >
              <option value="">{t("panelSidebar.selectExtension")}</option>
              {extensionPanels.map((panel) => (
                <option key={panel.extensionId} value={panel.extensionId}>
                  {panel.title}
                </option>
              ))}
            </Select>
            {errors.extensionId && (
              <label className="floorp-field-label">
                <span className="floorp-field-hint text-error">
                  {errors.extensionId}
                </span>
              </label>
            )}
          </div>
        )}

        <div className="floorp-field">
          <label className="floorp-field-label" htmlFor="panel-icon">
            <span className="floorp-field-text">{t("panelSidebar.icon")}</span>
          </label>
          <Input
            type="text"
            name="icon" id="panel-icon" aria-label={t("panelSidebar.icon")}
            value={editedPanel.icon || ""}
            onChange={handleChange}
            placeholder="chrome://browser/skin/preferences/icon.svg"
            className="w-full"
          />
        </div>

        <div className="floorp-field">
          <label className="floorp-field-label" htmlFor="panel-width">
            <span className="floorp-field-text">{t("panelSidebar.width")} (px)</span>
          </label>
          <Input
            type="number"
            name="width" id="panel-width" aria-label={t("panelSidebar.width")}
            value={editedPanel.width}
            onChange={handleChange}
            className="w-full"
          />
          {errors.width && (
            <label className="floorp-field-label">
              <span className="floorp-field-hint text-error">
                {errors.width}
              </span>
            </label>
          )}
        </div>

        {shouldShowWebFields && (
          <>
            <div className="floorp-field">
              <label className="floorp-field-label" htmlFor="panel-zoomLevel">
                <span className="floorp-field-text">
                  {t("panelSidebar.zoomLevel")}
                </span>
              </label>
              <Input
                type="number"
                name="zoomLevel" id="panel-zoomLevel" aria-label={t("panelSidebar.zoomLevel")}
                step="0.1"
                value={editedPanel.zoomLevel ?? ""}
                onChange={handleChange}
                placeholder="1.0"
                className="w-full"
              />
            </div>

            <div className="floorp-field">
              <label className="floorp-field-label" htmlFor="panel-userContextId">
                <span className="floorp-field-text">
                  {t("panelSidebar.container")}
                </span>
              </label>
              <Select
                name="userContextId" id="panel-userContextId" aria-label={t("panelSidebar.container")}
                value={editedPanel.userContextId?.toString() || "0"}
                onChange={handleChange}
                className="w-full"
              >
                <option value="0">{t("panelSidebar.noContainer")}</option>
                {containers.map((container) => (
                  <option key={container.id} value={container.id.toString()}>
                    {container.label}
                  </option>
                ))}
              </Select>
            </div>

            <div className="floorp-field">
              <label className="cursor-pointer floorp-field-label justify-start gap-2">
                <input
                  type="checkbox"
                  name="userAgent"
                  checked={editedPanel.userAgent === true}
                  onChange={handleCheckboxChange}
                  className="floorp-checkbox"
                />
                <span className="floorp-field-text">
                  {t("panelSidebar.useUserAgent")}
                </span>
              </label>
            </div>
          </>
        )}

        <div className="floorp-form-actions mt-6">
          <Button type="button" onClick={onClose} variant="secondary">
            {t("panelSidebar.cancel")}
          </Button>
          <Button type="submit" variant="primary">
            {t("panelSidebar.save")}
          </Button>
        </div>
      </form>
    </Modal>
  );
};
