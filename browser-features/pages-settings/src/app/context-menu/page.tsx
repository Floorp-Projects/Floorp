/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import { Menu, RotateCcw } from "lucide-react";
import { Button } from "@/components/common/button.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { ContextMenuEditor } from "./components/ContextMenuEditor.tsx";
import { useContextMenuSettings } from "./dataManager.ts";

export default function ContextMenuSettings() {
  const { t } = useTranslation();
  const model = useContextMenuSettings();
  const [confirmResetAll, setConfirmResetAll] = useState(false);

  return (
    <div className="space-y-3 p-6">
      <div className="flex flex-col items-start pl-6">
        <h1 className="mb-2 flex items-center gap-3 text-3xl font-bold">
          <Menu className="size-7" />
          {t("pages.contextMenu")}
        </h1>
        <p className="mb-8 text-sm">
          {t("contextMenu.description")}
        </p>
      </div>

      <div className="space-y-3 pl-6">
        <div
          className="min-h-6 text-sm"
          aria-live="polite"
          aria-atomic="true"
        >
          {model.pending && (
            <p role="status" className="text-base-content/70">
              {t("contextMenu.saving")}
            </p>
          )}
          {model.saveError && (
            <p role="alert" className="text-error">
              {t("contextMenu.saveError")}
            </p>
          )}
          {model.loadError && (
            <p role="alert" className="text-warning">
              {t("contextMenu.loadError")}
            </p>
          )}
        </div>

        <Card>
          <CardHeader>
            <CardTitle>{t("contextMenu.general")}</CardTitle>
            <CardDescription>
              {t("contextMenu.generalDescription")}
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between gap-4 rounded-lg bg-base-100 p-4">
              <div>
                <p className="font-medium">{t("contextMenu.enable")}</p>
                <p className="mt-1 text-sm text-base-content/60">
                  {t("contextMenu.enableDescription")}
                </p>
              </div>
              <Switch
                checked={model.enabled}
                disabled={model.loading || model.loadError}
                onChange={() => void model.toggleEnabled()}
                aria-label={t("contextMenu.enable")}
              />
            </div>

            <div className="flex flex-col gap-3 rounded-lg bg-base-100 p-4 sm:flex-row sm:items-center sm:justify-between">
              <div>
                <p className="font-medium">{t("contextMenu.resetAll")}</p>
                <p className="mt-1 text-sm text-base-content/60">
                  {t("contextMenu.resetAllDescription")}
                </p>
              </div>
              {confirmResetAll
                ? (
                  <div className="flex shrink-0 gap-2">
                    <Button
                      type="button"
                      size="sm"
                      variant="ghost"
                      onClick={() => setConfirmResetAll(false)}
                    >
                      {t("contextMenu.cancel")}
                    </Button>
                    <Button
                      type="button"
                      size="sm"
                      variant="danger"
                      disabled={model.loading || model.loadError}
                      onClick={() => {
                        setConfirmResetAll(false);
                        void model.resetAll();
                      }}
                    >
                      {t("contextMenu.confirmReset")}
                    </Button>
                  </div>
                )
                : (
                  <Button
                    type="button"
                    size="sm"
                    variant="light"
                    disabled={model.loading || model.loadError}
                    onClick={() => setConfirmResetAll(true)}
                  >
                    <RotateCcw className="mr-2 size-4" />
                    {t("contextMenu.resetAll")}
                  </Button>
                )}
            </div>
          </CardContent>
        </Card>

        {model.catalogLoading
          ? (
            <Card>
              <CardContent className="py-8 text-center text-sm text-base-content/60">
                <span className="loading loading-spinner loading-sm mr-2" />
                {t("contextMenu.loadingCatalog")}
              </CardContent>
            </Card>
          )
          : !model.catalog || model.catalog.surfaces.length === 0
          ? (
            <Card>
              <CardHeader>
                <CardTitle>{t("contextMenu.catalogUnavailable")}</CardTitle>
                <CardDescription>
                  {model.catalogError
                    ? t("contextMenu.catalogErrorDescription")
                    : t("contextMenu.emptyCatalogDescription")}
                </CardDescription>
              </CardHeader>
              <CardContent>
                <Button
                  type="button"
                  size="sm"
                  variant="light"
                  onClick={() => void model.reloadCatalog()}
                >
                  {t("contextMenu.retry")}
                </Button>
              </CardContent>
            </Card>
          )
          : (
            <ContextMenuEditor
              catalog={model.catalog}
              config={model.config}
              disabled={model.loading || model.loadError}
              reloadCatalog={model.reloadCatalog}
              moveItem={model.moveItem}
              moveItemBefore={model.moveItemBefore}
              setItemVisible={model.setItemVisible}
              setProfileIndependent={model.setProfileIndependent}
              resetProfile={model.resetProfile}
            />
          )}
      </div>
    </div>
  );
}
