/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import { useTranslation } from "react-i18next";
import { useMouseGestureConfig } from "./dataManager.ts";
import { GeneralSettings } from "./components/Preferences.tsx";
import { ActionsSettings } from "./components/ActionsSettings.tsx";

export default function Page() {
  const { t } = useTranslation();
  const {
    config,
    loading,
    pending,
    error,
    updateConfig,
    toggleEnabled,
    addAction,
    updateAction,
    deleteAction,
    updateRockerAction,
    updateWheelAction,
  } = useMouseGestureConfig();

  if (loading) {
    return <div className="py-6 text-center">{t("loading")}...</div>;
  }

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">
          {t("pages.mouseGesture")}
        </h1>
        <p className="floorp-page-description">
          {t("mouseGesture.description")}
        </p>
      </div>

      <div className={styles.sections}>
        <div
          className="text-sm empty:hidden"
          aria-live="polite"
          aria-atomic="true"
        >
          {pending && (
            <p role="status" className="text-base-content/70">
              {t("mouseGesture.saving")}
            </p>
          )}
          {error && (
            <p role="alert" className="text-error">
              {t("mouseGesture.saveError")}
            </p>
          )}
        </div>

        <GeneralSettings
          config={config}
          toggleEnabled={toggleEnabled}
          updateConfig={updateConfig}
          updateRockerAction={updateRockerAction}
          updateWheelAction={updateWheelAction}
        />

        <ActionsSettings
          config={config}
          addAction={addAction}
          updateAction={updateAction}
          deleteAction={deleteAction}
        />
      </div>
    </div>
  );
}
