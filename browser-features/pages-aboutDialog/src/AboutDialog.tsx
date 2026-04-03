import { useEffect, useState, useRef, useMemo } from "preact/hooks";
import { useTranslation } from "react-i18next";

// deno-lint-ignore no-explicit-any
declare const ChromeUtils: any;
// deno-lint-ignore no-explicit-any
declare const Services: any;
// deno-lint-ignore no-explicit-any
declare const Ci: any;
// deno-lint-ignore no-explicit-any
declare const Cc: any;
// deno-lint-ignore no-explicit-any
declare const openHelpLink: any;

// ─── Helpers ──────────────────────────────────────────────────────────────────

function openExternalUrl(url: string) {
  try {
    const win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win) {
      try {
        win.openUILinkIn(url, "tab", { inBackground: false });
      } catch {
        win.gBrowser.addTab(url, {
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
        });
        win.gBrowser.selectedTab = win.gBrowser.tabs[win.gBrowser.tabs.length - 1];
      }
      win.focus();
    }
  } catch {
    // fallback: do nothing if no browser window
  }
}

// ─── Types ────────────────────────────────────────────────────────────────────

type UpdatePanel =
  | "idle"
  | "checking"
  | "checkFailed"
  | "upToDate"
  | "available"
  | "downloading"
  | "applying"
  | "readyToRestart"
  | "restarting"
  | "policyDisabled"
  | "downloadFailed"
  | "noUpdater";

interface UpdateStatus {
  panel: UpdatePanel;
  downloadProgress?: string;
  availableVersion?: string;
}

// ─── Update status → human-readable text ──────────────────────────────────────

function updateLabel(
  status: UpdateStatus,
  t: (key: string, opts?: Record<string, unknown>) => string,
): { text: string; busy?: boolean } {
  switch (status.panel) {
    case "checking":
      return { text: t("aboutDialog.update.checking"), busy: true };
    case "checkFailed":
      return { text: t("aboutDialog.update.checkFailed") };
    case "upToDate":
      return { text: t("aboutDialog.update.upToDate") };
    case "downloading":
      return {
        text: t("aboutDialog.update.downloading", {
          progress: status.downloadProgress ?? "",
        }),
        busy: true,
      };
    case "applying":
      return { text: t("aboutDialog.update.applying"), busy: true };
    case "readyToRestart":
      return { text: t("aboutDialog.update.readyToRestart") };
    case "restarting":
      return { text: t("aboutDialog.update.restarting"), busy: true };
    case "policyDisabled":
      return { text: t("aboutDialog.update.policyDisabled") };
    case "downloadFailed":
      return { text: t("aboutDialog.update.downloadFailed") };
    case "noUpdater":
      return { text: t("aboutDialog.update.noUpdater") };
    default:
      return { text: "" };
  }
}

// ─── Component ────────────────────────────────────────────────────────────────

export function AboutDialog() {
  const { t } = useTranslation();

  const [distroAbout, setDistroAbout] = useState("");
  const [distroId, setDistroId] = useState("");
  const [version, setVersion] = useState("");
  const [isNightly, setIsNightly] = useState(false);
  const [isEsr, setIsEsr] = useState(false);
  const [releaseNotesUrl, setReleaseNotesUrl] = useState("");
  const [updateStatus, setUpdateStatus] = useState<UpdateStatus>({
    panel: "noUpdater",
  });

  // deno-lint-ignore no-explicit-any
  const appUpdaterRef = useRef<any>(null);
  const actionButtonRef = useRef<HTMLButtonElement>(null);

  const AppConstants = useMemo(() => {
    try {
      const { AppConstants } = ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      );
      return AppConstants;
    } catch {
      return {
        MOZ_UPDATER: false,
        IS_ESR: false,
        MOZ_APP_VERSION_DISPLAY: "unknown",
      };
    }
  }, []);

  // ── Initialise ──────────────────────────────────────────────────────────────

  useEffect(() => {
    const onKeydown = (e: KeyboardEvent) => {
      if (e.key === "Escape") globalThis.close();
    };
    globalThis.addEventListener("keydown", onKeydown);

    try {
      const defaults = Services.prefs.getDefaultBranch(null);
      const dId = defaults.getCharPref("distribution.id", "");
      if (dId) {
        const dAbout = defaults.getStringPref("distribution.about", "");
        const dVersion = defaults.getCharPref("distribution.version", "");
        if (dAbout) setDistroAbout(dAbout);
        if (!dId.startsWith("mozilla-") || dAbout)
          setDistroId(dVersion ? `${dId} — ${dVersion}` : dId);
      }

      const rawVersion = Services.appinfo.version;
      setVersion(AppConstants.MOZ_APP_VERSION_DISPLAY);
      if (/a\d+$/.test(rawVersion)) setIsNightly(true);
      if (AppConstants.IS_ESR) setIsEsr(true);

      if (
        Services.prefs.getPrefType("app.releaseNotesURL.aboutDialog") !==
        Services.prefs.PREF_INVALID
      ) {
        const url = Services.urlFormatter.formatURLPref(
          "app.releaseNotesURL.aboutDialog",
        );
        if (url !== "about:blank") setReleaseNotesUrl(url);
      }

      if (!AppConstants.MOZ_UPDATER) return;

      const { AppUpdater } = ChromeUtils.importESModule(
        "resource://gre/modules/AppUpdater.sys.mjs",
      );
      const { DownloadUtils } = ChromeUtils.importESModule(
        "resource://gre/modules/DownloadUtils.sys.mjs",
      );
      const updater = new AppUpdater();
      appUpdaterRef.current = updater;

      // deno-lint-ignore no-explicit-any
      const onUpdate = (status: any, ...args: any[]) => {
        switch (status) {
          case AppUpdater.STATUS.UPDATE_DISABLED_BY_POLICY:
            setUpdateStatus({ panel: "policyDisabled" });
            break;
          case AppUpdater.STATUS.READY_FOR_RESTART:
            setUpdateStatus({ panel: "readyToRestart" });
            break;
          case AppUpdater.STATUS.DOWNLOADING: {
            const [progress = 0] = args;
            const max = updater.update?.selectedPatch?.size ?? -1;
            setUpdateStatus({
              panel: "downloading",
              downloadProgress: DownloadUtils.getTransferTotal(progress, max),
            });
            break;
          }
          case AppUpdater.STATUS.STAGING:
            setUpdateStatus({ panel: "applying" });
            break;
          case AppUpdater.STATUS.CHECKING:
            setUpdateStatus({ panel: "checking" });
            break;
          case AppUpdater.STATUS.CHECKING_FAILED:
            setUpdateStatus({ panel: "checkFailed" });
            break;
          case AppUpdater.STATUS.NO_UPDATES_FOUND:
            setUpdateStatus({ panel: "upToDate" });
            break;
          case AppUpdater.STATUS.DOWNLOAD_AND_INSTALL:
            setUpdateStatus({
              panel: "available",
              availableVersion: updater.update?.displayVersion,
            });
            break;
          case AppUpdater.STATUS.DOWNLOAD_FAILED:
            setUpdateStatus({ panel: "downloadFailed" });
            break;
          case AppUpdater.STATUS.NEVER_CHECKED:
            setUpdateStatus({ panel: "idle" });
            break;
          default:
            setUpdateStatus({ panel: "noUpdater" });
        }
      };

      updater.addListener(onUpdate);
      updater.check();
      return () => {
        updater.removeListener(onUpdate);
        updater.stop();
        globalThis.removeEventListener("keydown", onKeydown);
      };
    // deno-lint-ignore no-empty
    } catch {}
    return () => globalThis.removeEventListener("keydown", onKeydown);
  }, [AppConstants]);

  useEffect(() => {
    actionButtonRef.current?.focus();
  }, [updateStatus.panel]);

  // ── Actions ─────────────────────────────────────────────────────────────────

  const checkForUpdates = () => appUpdaterRef.current?.check();
  const downloadUpdate = () => appUpdaterRef.current?.allowUpdateDownload();

  const restartToUpdate = () => {
    const aus = Cc["@mozilla.org/updates/update-service;1"].getService(
      Ci.nsIApplicationUpdateService,
    );
    if (aus.currentState !== Ci.nsIApplicationUpdateService.STATE_PENDING)
      return;

    setUpdateStatus({ panel: "restarting" });

    const cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool,
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart",
    );
    if (cancelQuit.data) {
      setUpdateStatus({ panel: "readyToRestart" });
      return;
    }

    if (Services.appinfo.inSafeMode) {
      Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
      return;
    }
    if (
      !Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart,
      )
    )
      setUpdateStatus({ panel: "readyToRestart" });
  };

  // ── Update UI ───────────────────────────────────────────────────────────────

  const renderUpdateSection = () => {
    if (updateStatus.panel === "idle")
      return (
        <button
          type="button"
          ref={actionButtonRef}
          class="ad-btn ad-btn-primary"
          onClick={checkForUpdates}
        >
          {t("aboutDialog.update.idle")}
        </button>
      );

    if (updateStatus.panel === "available")
      return (
        <button
          type="button"
          ref={actionButtonRef}
          class="ad-btn ad-btn-primary"
          onClick={downloadUpdate}
        >
          {t("aboutDialog.update.available", {
            version: updateStatus.availableVersion
              ? ` (${updateStatus.availableVersion})`
              : "",
          })}
        </button>
      );

    if (updateStatus.panel === "readyToRestart")
      return (
        <button
          type="button"
          ref={actionButtonRef}
          class="ad-btn ad-btn-primary"
          onClick={restartToUpdate}
        >
          {t("aboutDialog.update.restartToApply")}
        </button>
      );

    if (
      updateStatus.panel === "checkFailed" ||
      updateStatus.panel === "downloadFailed"
    )
      return (
        <div class="ad-error-col">
          <span role="alert" class="ad-status-err">
            {updateLabel(updateStatus, t).text}
          </span>
          <button
            type="button"
            ref={actionButtonRef}
            class="ad-btn ad-btn-ghost"
            onClick={checkForUpdates}
          >
            {t("aboutDialog.update.tryAgain")}
          </button>
        </div>
      );

    const { text, busy } = updateLabel(updateStatus, t);
    if (!text) return null;
    return (
      <div class="ad-update-row" aria-busy={busy}>
        {busy && <div class="ad-spinner" />}
        <span
          class={
            updateStatus.panel === "upToDate"
              ? "ad-status-ok"
              : "ad-status-text"
          }
        >
          {text}
        </span>
      </div>
    );
  };

  // ── Render ──────────────────────────────────────────────────────────────────

  const channelBadge = isNightly
    ? t("aboutDialog.channel.nightly")
    : isEsr
      ? t("aboutDialog.channel.esr")
      : null;

  return (
    <main class="ad" role="dialog" aria-label={t("aboutDialog.ariaLabel")}>
      {/* ── Two-column body ── */}
      <div class="ad-body">
        {/* ── Left: Brand + Update ── */}
        <div class="ad-left">
          <img
            src="chrome://branding/content/about-logo@2x.png"
            class="ad-logo"
            alt=""
            aria-hidden="true"
          />
          <h1 class="ad-name">Floorp</h1>
          <div class="ad-version">
            {t("aboutDialog.version")} {version || "…"}
            {channelBadge && (
              <span class="ad-badge">{channelBadge}</span>
            )}
          </div>
          <div class="ad-update-area" aria-live="polite" aria-atomic="true">
            {renderUpdateSection()}
          </div>
        </div>

        {/* ── Right: Community + Support ── */}
        <div class="ad-right">
          <section>
            <h2 class="ad-section-title">{t("aboutDialog.community.title")}</h2>
            <div class="ad-community-row">
              <a
                href="#"
                class="ad-community-item"
                onClick={(e) => {
                  e.preventDefault();
                  openExternalUrl("https://github.com/Floorp-Projects/Floorp");
                }}
              >
                <span class="ad-community-icon">
                  <svg viewBox="0 0 16 16"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.01 8.01 0 0016 8c0-4.42-3.58-8-8-8z" /></svg>
                </span>
                GitHub
              </a>
              <a
                href="#"
                class="ad-community-item"
                onClick={(e) => {
                  e.preventDefault();
                  openExternalUrl("https://discord.gg/floorp");
                }}
              >
                <span class="ad-community-icon">
                  <svg viewBox="0 0 16 16"><path d="M13.55 3.15A13.3 13.3 0 0010.23 2a9.5 9.5 0 00-.43.87 12.4 12.4 0 00-3.6 0A9.5 9.5 0 005.77 2a13.4 13.4 0 00-3.32 1.15C.35 6.4-.22 9.56.07 12.67A13.5 13.5 0 004.13 15a10 10 0 00.87-1.41 8.7 8.7 0 01-1.37-.66l.33-.26a9.6 9.6 0 008.08 0l.33.26c-.44.26-.9.48-1.37.66.25.5.54.97.87 1.41a13.4 13.4 0 004.06-2.33c.38-3.98-.65-7.1-2.98-10.52zM5.35 10.84c-.87 0-1.58-.82-1.58-1.82s.7-1.82 1.58-1.82 1.6.82 1.58 1.82c0 1-.7 1.82-1.58 1.82zm5.3 0c-.87 0-1.58-.82-1.58-1.82s.7-1.82 1.58-1.82 1.59.82 1.58 1.82c0 1-.7 1.82-1.58 1.82z" /></svg>
                </span>
                Discord
              </a>
              <a
                href="#"
                class="ad-community-item"
                onClick={(e) => {
                  e.preventDefault();
                  openExternalUrl("https://x.com/Floorp_Browser");
                }}
              >
                <span class="ad-community-icon">
                  <svg viewBox="0 0 16 16"><path d="M12.6.75h2.454l-5.36 6.142L16 15.25h-4.937l-3.867-5.07-4.425 5.07H.316l5.733-6.57L0 .75h5.063l3.495 4.633L12.601.75Zm-.86 13.028h1.36L4.323 2.145H2.865l8.875 11.633Z" /></svg>
                </span>
                X
              </a>
              <a
                href="#"
                class="ad-community-item"
                onClick={(e) => {
                  e.preventDefault();
                  openExternalUrl("https://www.reddit.com/r/floorp/");
                }}
              >
                <span class="ad-community-icon">
                  <svg viewBox="0 0 16 16"><path d="M16 8A8 8 0 1 1 0 8a8 8 0 0 1 16 0Zm-3.828-1.165a1.09 1.09 0 0 0-1.086.025 1.09 1.09 0 0 0-.478.387c-.96-.6-2.12-.95-3.33-.99l.67-3.02 2.16.48a.77.77 0 1 0 .08-.39l-2.38-.53a.2.2 0 0 0-.24.15l-.74 3.34c-1.26.02-2.46.38-3.45 1a1.09 1.09 0 0 0-1.82.82 1.09 1.09 0 0 0 .43.87c-.03.15-.04.3-.04.46 0 2.27 2.65 4.12 5.92 4.12s5.92-1.85 5.92-4.12c0-.16-.01-.31-.04-.46a1.09 1.09 0 0 0 .36-1.14ZM5.35 9a.77.77 0 1 1 0 1.54.77.77 0 0 1 0-1.54Zm4.56 2.87c-.56.56-1.63.6-1.91.6s-1.35-.05-1.91-.6a.2.2 0 0 1 .28-.28c.35.35 1.1.48 1.63.48s1.28-.13 1.63-.48a.2.2 0 0 1 .28.28Zm-.15-1.33a.77.77 0 1 1 0-1.54.77.77 0 0 1 0 1.54Z" /></svg>
                </span>
                Reddit
              </a>
            </div>
          </section>

          <section>
            <h2 class="ad-section-title">{t("aboutDialog.help.title")}</h2>
            <div class="ad-help-row">
              <a
                href="#"
                class="ad-help-link"
                onClick={(e) => {
                  e.preventDefault();
                  if (typeof openHelpLink === "function") {
                    openHelpLink("firefox-help");
                  }
                }}
                data-l10n-id="aboutdialog-help-user"
              >
                {t("aboutDialog.links.help")}
              </a>
              <a
                href="#"
                class="ad-help-link"
                onClick={(e) => {
                  e.preventDefault();
                  openExternalUrl("https://docs.floorp.app/docs/features/");
                }}
              >
                {t("aboutDialog.help.support")}
              </a>
            </div>
          </section>

          <section>
            <h2 class="ad-section-title">{t("aboutDialog.sponsor.title")}</h2>
            <a
              href="#"
              class="ad-btn ad-btn-outline"
              onClick={(e) => {
                e.preventDefault();
                openExternalUrl("https://github.com/sponsors/Ryosuke-Asano");
              }}
            >
              {t("aboutDialog.sponsor.button")}
            </a>
          </section>
        </div>
      </div>

      {/* ── Footer ── */}
      <footer class="ad-footer">
        {(distroAbout || distroId) && (
          <div class="ad-footer-distro">
            {distroAbout && <span style={{ fontWeight: 600 }}>{distroAbout}</span>}
            {distroAbout && distroId && " — "}
            {distroId && <span>{distroId}</span>}
          </div>
        )}
        {releaseNotesUrl && (
          <a
            href="#"
            onClick={(e) => {
              e.preventDefault();
              openExternalUrl(releaseNotesUrl!);
            }}
            data-l10n-id="releaseNotes-link"
          >
            {t("aboutDialog.links.releaseNotes")}
          </a>
        )}
        <a
          href="#"
          onClick={(e) => {
            e.preventDefault();
            openExternalUrl("about:license");
          }}
          data-l10n-id="bottomLinks-license"
        >
          {t("aboutDialog.links.licensing")}
        </a>
        <a
          href="#"
          onClick={(e) => {
            e.preventDefault();
            openExternalUrl("https://ja.floorp.app/privacy");
          }}
          data-l10n-id="bottom-links-privacy"
        >
          {t("aboutDialog.links.privacy")}
        </a>
        <a
          href="#"
          onClick={(e) => {
            e.preventDefault();
            openExternalUrl("https://ja.floorp.app/terms");
          }}
          data-l10n-id="bottom-links-terms"
        >
          {t("aboutDialog.links.terms")}
        </a>
        <p class="ad-tm">{t("aboutDialog.trademark")}</p>
      </footer>
    </main>
  );
}
