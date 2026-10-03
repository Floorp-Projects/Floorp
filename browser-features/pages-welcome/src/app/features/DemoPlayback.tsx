import {
  createContext,
  type ReactNode,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";
import { Pause, Play, RotateCcw } from "lucide-react";
import { useTranslation } from "react-i18next";
import type { DemoPlaybackState } from "./types.ts";
import styles from "./demos.module.css";

const PlaybackContext = createContext<DemoPlaybackState>({
  phase: 0,
  revision: 0,
  manual: false,
});
export const useDemoPlayback = () => useContext(PlaybackContext);

export function DemoPlayback(
  { active, children, onComplete }: {
    active: boolean;
    children: ReactNode;
    onComplete: () => void;
  },
) {
  const { t } = useTranslation();
  const [elapsed, setElapsed] = useState(0);
  const [running, setRunning] = useState(() =>
    !matchMedia("(prefers-reduced-motion: reduce)").matches
  );
  const [manual, setManual] = useState(false);
  const [revision, setRevision] = useState(0);
  const elapsedRef = useRef(0);
  const completedRef = useRef(false);
  useEffect(() => {
    if (active && !manual && elapsed >= 10000 && !completedRef.current) {
      completedRef.current = true;
      onComplete();
    }
  }, [active, manual, elapsed, onComplete]);
  useEffect(() => {
    if (!active) setRunning(false);
  }, [active]);
  useEffect(() => {
    const media = matchMedia("(prefers-reduced-motion: reduce)");
    const stop = () => {
      if (media.matches) setRunning(false);
    };
    media.addEventListener("change", stop);
    return () => media.removeEventListener("change", stop);
  }, []);
  useEffect(() => {
    if (!active || !running || manual) return;
    const start = performance.now() - elapsedRef.current;
    const timer = setInterval(() => {
      const next = Math.min(10000, performance.now() - start);
      elapsedRef.current = next;
      setElapsed(next);
      if (next === 10000) setRunning(false);
    }, 100);
    return () => clearInterval(timer);
  }, [active, running, manual, revision]);
  const restart = () => {
    completedRef.current = false;
    elapsedRef.current = 0;
    setElapsed(0);
    setManual(false);
    setRevision((value) => value + 1);
    setRunning(true);
  };
  const interrupt = () => {
    setManual(true);
    setRunning(false);
  };
  const phase = elapsed < 2000
    ? 0
    : elapsed < 4000
    ? 1
    : elapsed < 6000
    ? 2
    : elapsed < 7000
    ? 3
    : 4;
  return (
    <div
      className={styles.playback}
      data-playback
      data-running={running && active}
      data-phase={phase}
    >
      <div
        className={styles.playbackStage}
        onPointerDownCapture={interrupt}
        onKeyDownCapture={interrupt}
      >
        <PlaybackContext.Provider value={{ phase, revision, manual }}>
          {children}
        </PlaybackContext.Provider>
      </div>
      <div className={styles.playbackControls}>
        <div className={styles.playbackProgress} aria-hidden="true">
          <span style={{ width: `${elapsed / 100}%` }} />
        </div>
        <button
          type="button"
          onClick={() =>
            manual || elapsed >= 10000
              ? restart()
              : setRunning((value) => !value)}
        >
          {running ? <Pause size={16} /> : <Play size={16} />}
          {t(running ? "demoPlayback.pause" : "demoPlayback.play")}
        </button>
        <button type="button" onClick={restart}>
          <RotateCcw size={16} />
          {t("featureDemo.replay")}
        </button>
      </div>
    </div>
  );
}
