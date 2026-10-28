// SPDX-License-Identifier: MPL-2.0
import { useCallback, useEffect, useRef, useState } from "react";
import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { Button } from "../../../../../libs/ui/button.tsx";
import { Input } from "../../../../../libs/ui/input.tsx";
import { Switch } from "../../../../../libs/ui/switch.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "../../../../../libs/ui/section.tsx";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import {
  backgroundTimeLabel,
  floorpBackgrounds,
} from "../../../../../libs/newtab/background-assets.ts";
import type {
  BackgroundSettings,
  NewTabSettings,
  NewTabSettingsPatch,
  NewTabSnapshot,
} from "../../../../../libs/newtab/types.ts";
import { prepareImageForStorage } from "../../../../pages-newtab/src/utils/imageCompression.ts";
import { getStartDisabled, setStartDisabled } from "./design-setting.ts";
import { NEWTAB_SETTINGS_ROUTE, resolveNewTabTab } from "./fields.ts";
import type {
  NewTabTab,
  SettingRowProps,
  SettingsSectionProps,
} from "./types.ts";
import styles from "./page.module.css";
import { RestartModal } from "../../components/common/restart-modal.tsx";

function Section({ title, description, children }: SettingsSectionProps) {
  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle>{title}</CardTitle>
        <CardDescription className={styles.description}>
          {description}
        </CardDescription>
      </CardHeader>
      <CardContent>{children}</CardContent>
    </Card>
  );
}
function Row({ id, title, hint, children }: SettingRowProps) {
  return (
    <div className={styles.row}>
      <label htmlFor={id}>
        <span>{title}</span>
        {hint && <small>{hint}</small>}
      </label>
      <div className={styles.control}>{children}</div>
    </div>
  );
}
function conflict(cause: unknown): boolean {
  return (
    cause !== null &&
    typeof cause === "object" &&
    "code" in cause &&
    cause.code === "CONFLICT"
  );
}
const tabs: readonly NewTabTab[] = ["general", "dial", "feed", "sponsors"];
const backgroundModes: readonly BackgroundSettings["type"][] = [
  "none",
  "random",
  "custom",
  "folderPath",
  "floorp",
];

export default function NewTabSettingsPage() {
  const { t } = useTranslation();
  const location = useLocation();
  const navigate = useNavigate();
  const tab = resolveNewTabTab(location.search);
  const [snapshot, setSnapshot] = useState<NewTabSnapshot | null>(null);
  const current = useRef<NewTabSnapshot | null>(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(false);
  const [loadError, setLoadError] = useState(false);
  const [message, setMessage] = useState("");
  const [startDisabled, updateStartDisabled] = useState<boolean | null>(null);
  const [restart, setRestart] = useState(false);
  const [intervalDraft, setIntervalDraft] = useState("30");
  const fileInput = useRef<HTMLInputElement>(null);
  const alive = useRef(true);
  const loadSequence = useRef(0);

  const load = useCallback(async () => {
    const sequence = ++loadSequence.current;
    try {
      const result = await newTabClient.call("snapshot", {});
      if (!alive.current || sequence !== loadSequence.current) return;
      const merged = current.current &&
          result.settings.revision < current.current.settings.revision
        ? { ...result, settings: current.current.settings }
        : result;
      current.current = merged;
      setSnapshot(merged);
      setLoadError(false);
      if (!result.private && !result.readOnly) {
        try {
          const disabled = await getStartDisabled();
          if (alive.current && sequence === loadSequence.current) {
            updateStartDisabled(disabled);
          }
        } catch {
          if (alive.current) updateStartDisabled(null);
        }
      }
    } catch {
      if (alive.current && sequence === loadSequence.current) {
        setLoadError(true);
      }
    } finally {
      if (alive.current && sequence === loadSequence.current) setLoading(false);
    }
  }, []);
  useEffect(() => {
    alive.current = true;
    void load();
    const unsubscribe = newTabClient.subscribe(() => void load());
    const focused = () => void load();
    globalThis.addEventListener("focus", focused);
    return () => {
      alive.current = false;
      unsubscribe();
      globalThis.removeEventListener("focus", focused);
    };
  }, [load]);
  useEffect(() => {
    if (snapshot) {
      setIntervalDraft(String(snapshot.settings.background.slideshowInterval));
    }
  }, [snapshot?.settings.background.slideshowInterval]);

  const patch = async (changes: NewTabSettingsPatch) => {
    const value = current.current;
    if (!value || value.readOnly || value.private || busy) return;
    setBusy(true);
    setMessage("");
    try {
      const settings = await newTabClient.call("patchSettings", {
        expectedRevision: value.settings.revision,
        changes,
      });
      if (alive.current) {
        current.current = { ...(current.current ?? value), settings };
        setSnapshot(current.current);
        setMessage(t("newtab.saved"));
      }
    } catch (cause) {
      if (alive.current) {
        setMessage(t(conflict(cause) ? "newtab.conflict" : "ui.saveError"));
      }
      await load();
    } finally {
      if (alive.current) setBusy(false);
    }
  };
  const change = <K extends keyof NewTabSettingsPatch>(
    key: K,
    value: NewTabSettingsPatch[K],
  ) => void patch({ [key]: value });
  const changeBackground = (values: Partial<BackgroundSettings>) =>
    change("background", values);
  const chooseImage = async (file?: File) => {
    if (!file || busy) return;
    setBusy(true);
    setMessage("");
    try {
      if (
        !/^image\/(png|jpeg|webp|gif|avif)$/.test(file.type) ||
        file.size > 20 * 1024 * 1024
      ) {
        throw new Error("Unsupported image");
      }
      const image = await prepareImageForStorage(file);
      if (image.length > 3_500_000) {
        throw new Error("Image exceeds storage limit");
      }
      const decoded = new Image();
      decoded.src = image;
      await decoded.decode();
      const value = current.current;
      if (!value || value.private || value.readOnly) return;
      const settings = await newTabClient.call("patchSettings", {
        expectedRevision: value.settings.revision,
        changes: {
          background: {
            type: "custom",
            customImage: image,
            fileName: file.name,
          },
        },
      });
      if (alive.current) {
        current.current = { ...value, settings };
        setSnapshot(current.current);
        setMessage(t("newtab.saved"));
      }
    } catch {
      if (alive.current) setMessage(t("newtab.background.imageError"));
    } finally {
      if (alive.current) setBusy(false);
      if (fileInput.current) fileInput.current.value = "";
    }
  };
  const chooseFolder = async () => {
    if (busy) return;
    setBusy(true);
    setMessage("");
    try {
      const result = await newTabClient.call("pickFolder", {});
      const value = current.current;
      if (result.path && value) {
        const settings = await newTabClient.call("patchSettings", {
          expectedRevision: value.settings.revision,
          changes: {
            background: { type: "folderPath", folderPath: result.path },
          },
        });
        if (alive.current) {
          current.current = { ...value, settings };
          setSnapshot(current.current);
          setMessage(t("newtab.saved"));
        }
      }
    } catch {
      if (alive.current) setMessage(t("ui.saveError"));
    } finally {
      if (alive.current) setBusy(false);
    }
  };
  const toggleStart = async (disabled: boolean) => {
    setBusy(true);
    setMessage("");
    try {
      await setStartDisabled(disabled);
      if (alive.current) {
        updateStartDisabled(disabled);
        setRestart(true);
        setMessage(t("newtab.saved"));
      }
    } catch {
      if (alive.current) setMessage(t("ui.saveError"));
    } finally {
      if (alive.current) setBusy(false);
    }
  };
  const restoreSponsors = async () => {
    setBusy(true);
    setMessage("");
    try {
      const settings = await newTabClient.call("restoreSponsors", {});
      const value = current.current;
      if (alive.current && value) {
        current.current = { ...value, settings };
        setSnapshot(current.current);
        setMessage(t("newtab.saved"));
      }
    } catch {
      if (alive.current) setMessage(t("ui.saveError"));
    } finally {
      if (alive.current) setBusy(false);
    }
  };
  const selectTab = (value: NewTabTab) =>
    navigate(`${NEWTAB_SETTINGS_ROUTE}?tab=${value}`);
  const settings = snapshot?.settings;
  const slideshowEligible = settings?.background.type === "random" ||
    settings?.background.type === "folderPath";
  const unreadable = snapshot?.readOnly || snapshot?.private;

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      {restart && (
        <RestartModal
          onClose={() => setRestart(false)}
          label={t("newtab.restartRequired")}
        />
      )}
      <header className="floorp-page-header">
        <h1 className="floorp-page-heading">{t("newtab.title")}</h1>
        <p className="floorp-page-description">{t("newtab.description")}</p>
      </header>
      {loading && <p role="status">{t("ui.loading")}</p>}
      {loadError && (
        <div className={styles.notice}>
          <p role="alert">{t("ui.loadError")}</p>
          <Button
            variant="secondary"
            onClick={() => {
              setLoading(true);
              void load();
            }}
          >
            {t("ui.retry")}
          </Button>
        </div>
      )}
      {unreadable && (
        <p role="alert">
          {t(snapshot?.private ? "newtab.private" : "newtab.readOnly")}
        </p>
      )}
      {snapshot && settings && !unreadable && (
        <>
          <nav
            className={styles.tabs}
            aria-label={t("newtab.categories")}
            onKeyDown={(event) => {
              const index = tabs.indexOf(tab);
              const next = event.key === "ArrowRight"
                ? (index + 1) % tabs.length
                : event.key === "ArrowLeft"
                ? (index + tabs.length - 1) % tabs.length
                : event.key === "Home"
                ? 0
                : event.key === "End"
                ? tabs.length - 1
                : -1;
              if (next >= 0) {
                event.preventDefault();
                selectTab(tabs[next]);
                requestAnimationFrame(() =>
                  document.getElementById(`newtab-tab-${tabs[next]}`)?.focus()
                );
              }
            }}
          >
            {tabs.map((id) => (
              <button
                key={id}
                id={`newtab-tab-${id}`}
                type="button"
                aria-current={tab === id ? "page" : undefined}
                onClick={() => selectTab(id)}
              >
                {t(`newtab.tabs.${id}`)}
              </button>
            ))}
          </nav>
          <p
            className={styles.status}
            role="status"
            aria-live="polite"
            aria-atomic="true"
          >
            {busy ? t("newtab.saving") : message}
          </p>
          <fieldset
            disabled={busy || loading}
            aria-busy={busy || loading}
            className={styles.sections}
          >
            {tab === "general" && (
              <>
                <Section
                  title={t("newtab.visibility")}
                  description={t("newtab.visibilityDescription")}
                >
                  <Row id="newtab-search" title={t("newtab.search")}>
                    <Switch
                      id="newtab-search"
                      checked={settings.components.searchBar}
                      onChange={(e) =>
                        change("components", {
                          searchBar: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  <Row id="newtab-clock" title={t("newtab.clock")}>
                    <Switch
                      id="newtab-clock"
                      checked={settings.components.clock}
                      onChange={(e) =>
                        change("components", {
                          clock: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  <Row id="newtab-dials" title={t("newtab.dials")}>
                    <Switch
                      id="newtab-dials"
                      checked={settings.speedDial.show}
                      onChange={(e) =>
                        change("speedDial", { show: e.currentTarget.checked })}
                    />
                  </Row>
                  <Row id="newtab-history" title={t("newtab.history")}>
                    <Switch
                      id="newtab-history"
                      checked={settings.home.showHistory}
                      onChange={(e) =>
                        change("home", {
                          showHistory: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  <Row id="newtab-feed" title={t("newtab.feed")}>
                    <Switch
                      id="newtab-feed"
                      checked={settings.home.showFeed}
                      onChange={(e) =>
                        change("home", { showFeed: e.currentTarget.checked })}
                    />
                  </Row>
                </Section>
                <Section
                  title={t("newtab.layout")}
                  description={t("newtab.layoutDescription")}
                >
                  <Row
                    id="newtab-placement"
                    title={t("newtab.placement")}
                    hint={t("newtab.placementDescription")}
                  >
                    <select
                      id="newtab-placement"
                      className={styles.select}
                      value={String(settings.components.firefoxLayout)}
                      onChange={(e) =>
                        change("components", {
                          firefoxLayout: e.currentTarget.value === "true",
                        })}
                    >
                      <option value="true">{t("newtab.centered")}</option>
                      <option value="false">{t("newtab.sitesFirst")}</option>
                    </select>
                  </Row>
                  <Row id="newtab-layout" title={t("newtab.contentLayout")}>
                    <select
                      id="newtab-layout"
                      className={styles.select}
                      value={settings.home.layout}
                      onChange={(e) =>
                        change("home", {
                          layout: e.currentTarget
                            .value as NewTabSettings["home"]["layout"],
                        })}
                    >
                      {(["balanced", "links", "reading"] as const).map(
                        (value) => (
                          <option key={value} value={value}>
                            {t(`newtab.layouts.${value}`)}
                          </option>
                        ),
                      )}
                    </select>
                  </Row>
                </Section>
                <Section
                  title={t("newtab.background.title")}
                  description={t("newtab.background.description")}
                >
                  <fieldset className={styles.backgroundChoices}>
                    <legend className={styles.srOnly}>
                      {t("newtab.background.title")}
                    </legend>
                    {backgroundModes.map((mode, index) => (
                      <label key={mode} className={styles.backgroundChoice}>
                        <input
                          id={index === 0
                            ? "newtab-background-type"
                            : undefined}
                          type="radio"
                          name="newtab-background"
                          value={mode}
                          checked={settings.background.type === mode}
                          onChange={() =>
                            changeBackground({
                              type: mode,
                              ...(mode === "floorp" &&
                                  !settings.background.selectedFloorp
                                ? {
                                  selectedFloorp: floorpBackgrounds[0]?.name ??
                                    null,
                                }
                                : {}),
                            })}
                        />
                        <span>{t(`newtab.background.modes.${mode}`)}</span>
                      </label>
                    ))}
                  </fieldset>
                  <Row
                    id="newtab-transparency"
                    title={t("newtab.transparency")}
                    hint={t("newtab.transparencyDescription")}
                  >
                    <Switch
                      id="newtab-transparency"
                      checked={settings.home.translucentPanels}
                      disabled={settings.background.type === "none"}
                      onChange={(e) =>
                        change("home", {
                          translucentPanels: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  {settings.background.type === "custom" && (
                    <div className={styles.backgroundDetail}>
                      <p>
                        {settings.background.fileName ||
                          t("newtab.background.noImage")}
                      </p>
                      <Button
                        variant="secondary"
                        onClick={() => fileInput.current?.click()}
                      >
                        {t("newtab.background.chooseImage")}
                      </Button>
                      <input
                        ref={fileInput}
                        type="file"
                        accept="image/png,image/jpeg,image/webp,image/gif,image/avif"
                        hidden
                        onChange={(e) =>
                          void chooseImage(e.currentTarget.files?.[0])}
                      />
                    </div>
                  )}
                  {settings.background.type === "folderPath" && (
                    <div className={styles.backgroundDetail}>
                      <p>
                        {settings.background.folderPath ||
                          t("newtab.background.noFolder")}
                      </p>
                      <Button
                        variant="secondary"
                        onClick={() =>
                          void chooseFolder()}
                      >
                        {t("newtab.background.chooseFolder")}
                      </Button>
                    </div>
                  )}
                  {settings.background.type === "floorp" && (
                    <fieldset className={styles.gallery}>
                      <legend className={styles.srOnly}>
                        {t("newtab.background.selectFloorp")}
                      </legend>
                      {floorpBackgrounds.map((image) => (
                        <label
                          key={image.name}
                          className={styles.galleryChoice}
                          data-selected={settings.background.selectedFloorp ===
                            image.name}
                        >
                          <input
                            type="radio"
                            name="floorp-image"
                            value={image.name}
                            checked={settings.background.selectedFloorp ===
                              image.name}
                            onChange={() =>
                              changeBackground({ selectedFloorp: image.name })}
                          />
                          <img src={image.url} alt="" />
                          <span>{backgroundTimeLabel(image.name)}</span>
                        </label>
                      ))}
                    </fieldset>
                  )}
                  <Row
                    id="newtab-slideshow"
                    title={t("newtab.background.slideshow")}
                    hint={t("newtab.background.slideshowDescription")}
                  >
                    <Switch
                      id="newtab-slideshow"
                      disabled={!slideshowEligible}
                      checked={settings.background.slideshowEnabled}
                      onChange={(e) =>
                        changeBackground({
                          slideshowEnabled: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  <Row
                    id="newtab-interval"
                    title={t("newtab.background.interval")}
                  >
                    <div className={styles.interval}>
                      <Input
                        id="newtab-interval"
                        type="number"
                        min={1}
                        max={86400}
                        disabled={!slideshowEligible ||
                          !settings.background.slideshowEnabled}
                        value={intervalDraft}
                        onChange={(e) =>
                          setIntervalDraft(e.currentTarget.value)}
                        onBlur={() => {
                          const seconds = Number(intervalDraft);
                          if (
                            Number.isInteger(seconds) &&
                            seconds >= 1 &&
                            seconds <= 86400 &&
                            seconds !== settings.background.slideshowInterval
                          ) {
                            changeBackground({ slideshowInterval: seconds });
                          } else {
                            setIntervalDraft(
                              String(settings.background.slideshowInterval),
                            );
                          }
                        }}
                      />
                      <span>{t("newtab.background.seconds")}</span>
                    </div>
                  </Row>
                </Section>
                <Section
                  title={t("newtab.history")}
                  description={t("newtab.blockedDescription")}
                >
                  {settings.topSites.blocked.length
                    ? (
                      <ul className={styles.blocked}>
                        {settings.topSites.blocked.map((url) => (
                          <li key={url}>
                            <span>{url}</span>
                            <Button
                              variant="secondary"
                              onClick={() =>
                                change("topSites", {
                                  blocked: settings.topSites.blocked.filter(
                                    (value) => value !== url,
                                  ),
                                })}
                            >
                              {t("newtab.restore")}
                            </Button>
                          </li>
                        ))}
                      </ul>
                    )
                    : <p className={styles.note}>{t("newtab.noBlocked")}</p>}
                </Section>
                <Section
                  title={t("newtab.floorpStart")}
                  description={t("newtab.floorpStartDescription")}
                >
                  <Row
                    id="disable-floorp-start"
                    title={t(
                      "design.uiCustomization.newtab.disableFloorpStart",
                    )}
                    hint={startDisabled === null
                      ? t("ui.loadError")
                      : t("newtab.restartRequired")}
                  >
                    <Switch
                      id="disable-floorp-start"
                      disabled={startDisabled === null}
                      checked={startDisabled ?? false}
                      onChange={(e) =>
                        void toggleStart(e.currentTarget.checked)}
                    />
                  </Row>
                </Section>
              </>
            )}
            {tab === "dial" && (
              <>
                <Section
                  title={t("newtab.tileDisplay")}
                  description={t("newtab.tileDescription")}
                >
                  <Row id="newtab-columns" title={t("newtab.columns")}>
                    <select
                      id="newtab-columns"
                      className={styles.select}
                      value={settings.speedDial.columns}
                      onChange={(e) =>
                        change("speedDial", {
                          columns: Number(e.currentTarget.value) as 4 | 5 | 6,
                        })}
                    >
                      {[4, 5, 6].map((value) => (
                        <option key={value} value={value}>
                          {t("newtab.columnCount", { count: value })}
                        </option>
                      ))}
                    </select>
                  </Row>
                  <Row id="newtab-tile-size" title={t("newtab.tileSize")}>
                    <select
                      id="newtab-tile-size"
                      className={styles.select}
                      value={settings.speedDial.tileSize}
                      onChange={(e) =>
                        change("speedDial", {
                          tileSize: e.currentTarget.value as
                            | "standard"
                            | "compact",
                        })}
                    >
                      <option value="standard">{t("newtab.standard")}</option>
                      <option value="compact">{t("newtab.compact")}</option>
                    </select>
                  </Row>
                </Section>
                <Section
                  title={t("newtab.workspaceGroups")}
                  description={t("newtab.workspaceGroupsDescription")}
                >
                  <div id="newtab-workspace-groups" tabIndex={-1}>
                    {snapshot.workspaces.length
                      ? (
                        snapshot.workspaces.map((workspace) => (
                          <Row
                            key={workspace.id}
                            id={`newtab-group-${workspace.id}`}
                            title={workspace.title}
                          >
                            <select
                              id={`newtab-group-${workspace.id}`}
                              className={styles.select}
                              value={settings.speedDial.workspaceGroups[
                                workspace.id
                              ] ??
                                settings.speedDial.commonGuid ??
                                ""}
                              onChange={(e) =>
                                change("speedDial", {
                                  workspaceGroups: {
                                    ...settings.speedDial.workspaceGroups,
                                    [workspace.id]: e.currentTarget.value,
                                  },
                                })}
                            >
                              {snapshot.groups.map((group) => (
                                <option key={group.guid} value={group.guid}>
                                  {group.title}
                                </option>
                              ))}
                            </select>
                          </Row>
                        ))
                      )
                      : (
                        <p className={styles.note}>
                          {t("newtab.noWorkspaces")}
                        </p>
                      )}
                  </div>
                  <p className={styles.note}>
                    {t("newtab.newWorkspaceCommon")}
                  </p>
                </Section>
              </>
            )}
            {tab === "feed" && (
              <>
                <Section
                  title={t("newtab.feedUpdate")}
                  description={t("newtab.feedUpdateDescription")}
                >
                  <Row id="newtab-refresh" title={t("newtab.refresh")}>
                    <select
                      id="newtab-refresh"
                      className={styles.select}
                      value={settings.rss.refreshMinutes}
                      onChange={(e) =>
                        change("rss", {
                          refreshMinutes: Number(e.currentTarget.value) as
                            | 0
                            | 15
                            | 30
                            | 60,
                        })}
                    >
                      {[15, 30, 60].map((value) => (
                        <option key={value} value={value}>
                          {t("newtab.everyMinutes", { count: value })}
                        </option>
                      ))}
                      <option value={0}>{t("newtab.manualOnly")}</option>
                    </select>
                  </Row>
                </Section>
                <Section
                  title={t("newtab.reading")}
                  description={t("newtab.readingDescription")}
                >
                  <div className={styles.copy}>
                    <p>{t("newtab.feedBodyPolicy")}</p>
                    <p>{t("newtab.feedShared")}</p>
                    <p>
                      {t("newtab.feedRetention", {
                        days: settings.rss.retentionDays,
                        count: settings.rss.maxItems,
                      })}
                    </p>
                    <p>{t("newtab.feedImages")}</p>
                  </div>
                </Section>
              </>
            )}
            {tab === "sponsors" && (
              <>
                <Section
                  title={t("newtab.sponsors.title")}
                  description={t("newtab.sponsors.description")}
                >
                  <Row
                    id="newtab-sponsors"
                    title={t("newtab.sponsors.show")}
                    hint={t("newtab.sponsors.twoSlots")}
                  >
                    <Switch
                      id="newtab-sponsors"
                      checked={settings.sponsors.enabled}
                      onChange={(e) =>
                        change("sponsors", {
                          enabled: e.currentTarget.checked,
                        })}
                    />
                  </Row>
                  <p className={styles.note}>{t("newtab.sponsors.shared")}</p>
                </Section>
                <Section
                  title={t("newtab.sponsors.hidden")}
                  description={t("newtab.sponsors.hiddenDescription")}
                >
                  <p className={styles.note}>
                    {t("newtab.sponsors.hiddenCount", {
                      count: settings.sponsors.dismissedIds.length,
                    })}
                  </p>
                  <Button
                    id="newtab-restore-sponsors"
                    variant="secondary"
                    disabled={!settings.sponsors.dismissedIds.length}
                    onClick={() => void restoreSponsors()}
                  >
                    {t("newtab.sponsors.restore")}
                  </Button>
                </Section>
              </>
            )}
          </fieldset>
        </>
      )}
    </div>
  );
}
