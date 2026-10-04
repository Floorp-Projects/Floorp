import { useEffect, useId, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  AppWindow,
  ArrowLeft,
  ArrowRight,
  BookOpen,
  Briefcase,
  Calendar,
  Check,
  ChevronDown,
  Globe,
  House,
  Music,
  Play,
  Plus,
  RotateCcw,
  Settings,
  Star,
  X,
} from "lucide-react";
import styles from "./demos.module.css";
import { DemoChrome } from "./DemoChrome.tsx";
import { useDemoPlayback } from "./DemoPlayback.tsx";

const workspaceIcons = [Briefcase, House, BookOpen];

export function WorkspaceDemo() {
  const { t } = useTranslation();
  const [workspace, setWorkspace] = useState("0");
  const [open, setOpen] = useState(false);
  const [switched, setSwitched] = useState(false);
  const [highlight, setHighlight] = useState(false);
  const picker = useRef<HTMLDetailsElement>(null);
  const groupName = useId();
  const playback = useDemoPlayback();
  useEffect(() => {
    if (playback.manual) return;
    const changed = playback.phase >= 2;
    setWorkspace(changed ? "1" : "0");
    setSwitched(changed);
    setHighlight(changed);
    if (picker.current) picker.current.open = playback.phase === 1;
  }, [playback.phase, playback.revision, playback.manual]);
  useEffect(() => {
    if (!highlight) return;
    const timer = setTimeout(() => setHighlight(false), 1800);
    return () => clearTimeout(timer);
  }, [highlight, workspace]);
  useEffect(() => {
    const closeOutside = (event: Event) => {
      if (
        event.target instanceof Node && picker.current &&
        !picker.current.contains(event.target)
      ) {
        picker.current.open = false;
      }
    };
    document.addEventListener("click", closeOutside);
    document.addEventListener("keyup", closeOutside);
    return () => {
      document.removeEventListener("click", closeOutside);
      document.removeEventListener("keyup", closeOutside);
    };
  }, []);
  const Icon = workspaceIcons[Number(workspace)];
  return (
    <div className={styles.demo} data-demo="workspace">
      <p
        className={`${styles.instruction} ${styles.guideInstruction}`}
        role="status"
      >
        <span className={styles.guideNumber} aria-hidden="true">
          {switched ? 3 : open ? 2 : 1}
        </span>
        {t(
          switched
            ? "featureDemo.workspaces.guideDone"
            : open
            ? "featureDemo.workspaces.guideSelect"
            : "featureDemo.workspaces.guideOpen",
        )}
      </p>
      <div className={styles.browser} data-demo-viewport>
        <div className={styles.tabStrip} data-highlight={highlight}>
          {[0, 1, 2].map((index) => (
            <span key={index}>
              <Globe size={16} />
              {t(`featureDemo.workspaces.tabs.${workspace}.${index}`)}
              <X size={14} aria-hidden="true" />
            </span>
          ))}
        </div>
        <div className={styles.toolbar}>
          {/* Native disclosure avoids pointerdown cancellation in this system-principal page. */}
          <div className={styles.workspaceAnchor}>
            <details
              ref={picker}
              className={styles.workspacePicker}
              onToggle={(event) => setOpen(event.currentTarget.open)}
              onKeyDown={(event) => {
                if (event.key === "Escape") {
                  event.stopPropagation();
                  event.currentTarget.open = false;
                  event.currentTarget.querySelector("summary")?.focus();
                }
              }}
            >
              <summary className={styles.control} data-demo-action="workspace">
                <Icon size={22} />
                {t(`featureDemo.workspaces.names.${workspace}`)}
                <ChevronDown size={18} />
              </summary>
              <fieldset
                className={styles.menu}
                aria-label={t("featuresPage.workspaces.title")}
              >
                {!switched && (
                  <p className={styles.selectHint}>
                    {t("featureDemo.workspaces.selectHint")}
                  </p>
                )}
                {workspaceIcons.map((ItemIcon, index) => (
                  <label
                    key={index}
                    className={styles.menuItem}
                    data-next-target={!switched && index === 1}
                    data-state={workspace === String(index)
                      ? "checked"
                      : "unchecked"}
                  >
                    <input
                      type="radio"
                      name={groupName}
                      value={String(index)}
                      checked={workspace === String(index)}
                      onChange={() => {
                        setWorkspace(String(index));
                        setSwitched(true);
                        setHighlight(true);
                        if (picker.current) {
                          picker.current.open = false;
                          picker.current.querySelector("summary")?.focus();
                        }
                      }}
                    />
                    <ItemIcon size={22} />
                    <span>
                      {t(`featureDemo.workspaces.names.${index}`)}
                    </span>
                    {workspace === String(index) && (
                      <Check size={18} aria-hidden="true" />
                    )}
                  </label>
                ))}
              </fieldset>
            </details>
            {!open && !switched && (
              <span className={styles.openHint} aria-hidden="true">
                {t("featureDemo.workspaces.openHint")}
              </span>
            )}
          </div>
          <div className={styles.chromeIcons} aria-hidden="true">
            <ArrowLeft size={20} />
            <ArrowRight size={20} />
            <RotateCcw size={20} />
          </div>
          <div className={styles.address} aria-hidden="true">example.com</div>
        </div>
        <article
          className={styles.workspacePage}
          data-intro={!switched}
          data-page={workspace}
        >
          <div className={styles.samplePageHeader}>
            <Icon size={20} aria-hidden="true" />
            <span>{t(`featureDemo.workspaces.tabs.${workspace}.0`)}</span>
            <span className={styles.pageCategory}>
              {t(`featureDemo.workspaces.pages.${workspace}.category`)}
            </span>
          </div>
          <div className={styles.samplePageBody}>
            <div>
              <h4>{t(`featureDemo.workspaces.pages.${workspace}.title`)}</h4>
              <p>
                {t(`featureDemo.workspaces.pages.${workspace}.description`)}
              </p>
            </div>
            <ul className={styles.pageEntries}>
              {[0, 1, 2].map((index) => (
                <li key={index}>
                  <span aria-hidden="true">
                    {workspace === "0"
                      ? <Check size={15} />
                      : String(index + 1).padStart(2, "0")}
                  </span>
                  {t(
                    `featureDemo.workspaces.pages.${workspace}.items.${index}`,
                  )}
                </li>
              ))}
            </ul>
          </div>
        </article>
      </div>
      <p className={styles.caption} role="status">
        {switched && <Check size={18} aria-hidden="true" />}
        {t("featureDemo.workspaces.result", {
          name: t(`featureDemo.workspaces.names.${workspace}`),
        })}
      </p>
    </div>
  );
}

export function PanelDemo() {
  const { t } = useTranslation();
  const [open, setOpen] = useState(false);
  const [playing, setPlaying] = useState(false);
  const panelId = useId();
  const trigger = useRef<HTMLButtonElement>(null);
  const playback = useDemoPlayback();
  useEffect(() => {
    if (playback.manual) return;
    setOpen(playback.phase >= 2);
    setPlaying(false);
  }, [playback.phase, playback.revision, playback.manual]);
  return (
    <div className={styles.demo} data-demo="panel">
      <p className={styles.instruction}>
        {t("featureDemo.panelSidebar.instruction")}
      </p>
      <div className={styles.stageShell} data-demo-viewport>
        <DemoChrome title="Floorp" address="en.wikipedia.org/wiki/Floorp" />
        <div className={styles.panelBrowser} data-open={open}>
          <div className={styles.article}>
            <BookOpen size={30} />
            <strong>{t("featureDemo.panelSidebar.article")}</strong>
            <div className={styles.landscape} aria-hidden="true" />
            <div className={styles.documents} aria-hidden="true">
              <span />
              <span />
            </div>
          </div>
          {open && (
            <section
              className={styles.musicPanel}
              id={panelId}
              aria-label={t("featureDemo.panelSidebar.music")}
            >
              <div className={styles.panelHeader}>
                <div className={styles.panelTools} aria-hidden="true">
                  <ArrowLeft size={18} />
                  <ArrowRight size={18} />
                  <RotateCcw size={18} />
                  <House size={18} />
                </div>
                <button
                  type="button"
                  aria-label={t("featureDemo.close")}
                  onClick={() => {
                    setOpen(false);
                    trigger.current?.focus();
                  }}
                >
                  <X size={20} />
                </button>
              </div>
              <div className={styles.musicTitle}>
                <Music size={24} />
                <strong>{t("featureDemo.panelSidebar.music")}</strong>
              </div>
              <span className={styles.note}>
                {t("featureDemo.panelSidebar.favorites")}
              </span>
              {[0, 1, 2].map((index) => (
                <div className={styles.track} key={index}>
                  <span className={styles.album} data-index={index}>
                    <Music size={22} />
                  </span>
                  <span>{t(`featureDemo.panelSidebar.tracks.${index}`)}</span>
                </div>
              ))}
              <button
                type="button"
                className={styles.control}
                aria-pressed={playing}
                onClick={() => setPlaying(!playing)}
              >
                <Play size={18} />
                {t(
                  playing
                    ? "featureDemo.panelSidebar.stop"
                    : "featureDemo.panelSidebar.play",
                )}
              </button>
            </section>
          )}
          <div className={styles.rail}>
            <Star aria-hidden="true" size={22} />
            <button
              type="button"
              ref={trigger}
              aria-label={t("featureDemo.panelSidebar.toggle")}
              aria-expanded={open}
              aria-controls={open ? panelId : undefined}
              onClick={() => setOpen(!open)}
              data-active={open}
            >
              <Music size={24} />
            </button>
            <Plus size={22} aria-hidden="true" />
            <Settings size={22} aria-hidden="true" />
          </div>
        </div>
      </div>
      <p className={styles.caption} role="status">
        {t(
          open
            ? "featureDemo.panelSidebar.open"
            : "featureDemo.panelSidebar.closed",
        )}
      </p>
    </div>
  );
}

export function AppDemo() {
  const { t } = useTranslation();
  const [open, setOpen] = useState(false);
  const [installed, setInstalled] = useState(false);
  const trigger = useRef<HTMLButtonElement>(null);
  const panelId = useId();
  const playback = useDemoPlayback();
  useEffect(() => {
    if (playback.manual) return;
    setOpen(playback.phase === 1);
    setInstalled(playback.phase >= 2);
  }, [playback.phase, playback.revision, playback.manual]);
  function close() {
    setOpen(false);
    trigger.current?.focus();
  }
  return (
    <div
      className={styles.demo}
      data-demo="app"
      onKeyDown={(event) => {
        if (event.key === "Escape" && open) {
          event.stopPropagation();
          close();
        }
      }}
    >
      <p className={styles.instruction}>{t("featureDemo.pwa.instruction")}</p>
      <div className={styles.appStage} data-demo-viewport>
        <DemoChrome
          title={t("featureDemo.pwa.calendar")}
          address="calendar.example"
          action={
            <button
              type="button"
              ref={trigger}
              className={styles.control}
              aria-label={t("featureDemo.pwa.icon")}
              aria-expanded={open}
              aria-controls={open ? panelId : undefined}
              onClick={() => setOpen(!open)}
            >
              <AppWindow size={22} />
              <Plus size={14} />
            </button>
          }
        />
        {open && (
          <section
            className={styles.installPanel}
            id={panelId}
            aria-label={t("featureDemo.pwa.open")}
          >
            <strong>{t("featureDemo.pwa.open")}</strong>
            <div className={styles.siteIdentity}>
              <Calendar size={36} />
              <span>
                {t("featureDemo.pwa.calendar")}
                <small>calendar.example</small>
              </span>
            </div>
            <div className={styles.actions}>
              <button type="button" className={styles.control} onClick={close}>
                {t("featureDemo.cancel")}
              </button>
              <button
                type="button"
                className={styles.primary}
                onClick={() => {
                  setInstalled(true);
                  close();
                }}
              >
                {t(
                  installed
                    ? "featureDemo.pwa.open"
                    : "featureDemo.pwa.install",
                )}
              </button>
            </div>
          </section>
        )}
        {installed
          ? (
            <section
              className={styles.appWindow}
              aria-label={t("featureDemo.pwa.result")}
            >
              <div className={styles.panelHeader}>
                <Calendar size={22} />
                <strong>{t("featureDemo.pwa.calendar")}</strong>
                <span className={styles.windowControls} aria-hidden="true">
                  − □ ×
                </span>
              </div>
              <div className={styles.calendar} aria-hidden="true">
                {Array.from(
                  { length: 28 },
                  (_, i) => <span key={i} data-today={i === 13}>{i + 1}</span>,
                )}
              </div>
            </section>
          )
          : (
            <div className={styles.appPlaceholder}>
              <Calendar size={56} />
              <span>{t("featureDemo.pwa.calendar")}</span>
            </div>
          )}
      </div>
      <div className={styles.demoFooter}>
        {installed && (
          <>
            <span className={styles.caption} role="status">
              {t("featureDemo.pwa.result")}
            </span>
            <button
              type="button"
              className={styles.textButton}
              onClick={() => {
                setInstalled(false);
                setOpen(false);
                trigger.current?.focus();
              }}
            >
              <RotateCcw size={16} />
              {t("featureDemo.reset")}
            </button>
          </>
        )}
      </div>
    </div>
  );
}

export function GestureDemo() {
  const { t } = useTranslation();
  const [step, setStep] = useState(0);
  const playback = useDemoPlayback();
  useEffect(() => {
    if (!playback.manual) setStep(playback.phase);
  }, [playback.phase, playback.revision, playback.manual]);
  useEffect(() => {
    if (!playback.manual || step < 1 || step > 3) return;
    const timer = setTimeout(() => setStep(step + 1), 1100);
    return () => clearTimeout(timer);
  }, [step, playback.manual]);
  return (
    <div className={styles.demo} data-demo="gesture">
      <ol className={styles.gestureSteps}>
        {[1, 2, 3].map((value) => (
          <li key={value} data-active={step === value}>
            <span>{value}</span>
            {t(`featureDemo.mouseGesture.steps.${value - 1}`)}
          </li>
        ))}
      </ol>
      <div className={styles.stageShell} data-demo-viewport>
        <DemoChrome
          title={t(
            step === 4
              ? "featureDemo.mouseGesture.previous"
              : "featureDemo.mouseGesture.current",
          )}
          address="example.com"
        />
        <div className={styles.gestureStage} data-step={step}>
          <div className={styles.gesturePage}>
            <BookOpen size={32} />
            <strong>
              {t(
                step === 4
                  ? "featureDemo.mouseGesture.previous"
                  : "featureDemo.mouseGesture.current",
              )}
            </strong>
            <div className={styles.documents} aria-hidden="true">
              <span />
              <span />
              <span />
            </div>
            <div className={styles.trail} aria-hidden="true">
              <ArrowLeft size={32} />
            </div>
          </div>
          <div className={styles.mouse} aria-hidden="true">
            <span />
            <span />
            <i />
          </div>
        </div>
      </div>
      <div className={styles.demoFooter}>
        <div className={styles.actions}>
          <button
            type="button"
            className={styles.control}
            disabled={step > 0 && step < 4}
            onClick={() => setStep(1)}
          >
            <Play size={18} />
            {t(step === 4 ? "featureDemo.replay" : "featureDemo.play")}
          </button>
          <span className={styles.caption} role="status">
            {t(
              step === 4
                ? "featureDemo.mouseGesture.result"
                : "featureDemo.mouseGesture.hint",
            )}
          </span>
        </div>
        <p className={styles.note}>{t("featureDemo.mouseGesture.enable")}</p>
      </div>
    </div>
  );
}
