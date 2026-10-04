import { useEffect, useState } from "react";
import {
  ArrowRight,
  Check,
  Globe,
  Moon,
  Play,
  Rocket,
  Settings,
  Sun,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import { FloorpBrand } from "../../../../../libs/ui/brand.tsx";
import styles from "./welcome-scene.module.css";

const icons = [Globe, Settings, Rocket];

export function WelcomeScene() {
  const { t } = useTranslation();
  const [phase, setPhase] = useState(0);
  const [running, setRunning] = useState(true);
  const [reduced, setReduced] = useState(false);
  useEffect(() => {
    const media = matchMedia("(prefers-reduced-motion: reduce)");
    const update = () => {
      setReduced(media.matches);
      if (media.matches) {
        setRunning(false);
        setPhase(2);
      }
    };
    update();
    media.addEventListener("change", update);
    return () => media.removeEventListener("change", update);
  }, []);
  useEffect(() => {
    if (!running || reduced) return;
    const timer = setTimeout(() => {
      if (phase < 2) setPhase(phase + 1);
      else setRunning(false);
    }, 10000 / 3);
    return () => clearTimeout(timer);
  }, [phase, running, reduced]);
  return (
    <div
      className={styles.scene}
      data-welcome-scene
      data-phase={phase}
      data-running={running}
    >
      <div className={styles.art} aria-hidden="true">
        <div className={styles.route} />
        <div className={styles.browser}>
          <div className={styles.tabs}>
            <span />
            <span />
            <span />
            <i>＋</i>
          </div>
          <div className={styles.address}>
            <ArrowRight size={16} />
            <span />
            <i>⋯</i>
          </div>
          <div className={styles.canvas}>
            <FloorpBrand onDark />
            <div className={styles.search}>
              <Globe size={18} />
              <span />
              <ArrowRight size={18} />
            </div>
            <div className={styles.shortcuts}>
              <span>
                <Globe />
              </span>
              <span>
                <Settings />
              </span>
              <span>
                <Rocket />
              </span>
            </div>
          </div>
        </div>
        <div className={styles.language}>
          <Globe size={30} />
          <div>
            <strong>こんにちは</strong>
            <span>Hello · Bonjour</span>
          </div>
        </div>
        <div className={styles.appearance}>
          <Sun size={24} />
          <div className={styles.swatches}>
            <span />
            <span />
            <span />
          </div>
          <Moon size={24} />
        </div>
        <div className={styles.launch}>
          <Rocket size={34} />
          <Check size={20} />
        </div>
      </div>
      <div className={styles.steps}>
        {icons.map((Icon, index) => (
          <button
            key={index}
            type="button"
            aria-pressed={phase === index}
            onClick={() => {
              setRunning(false);
              setPhase(index);
            }}
          >
            <span className={styles.stepIcon}>
              <Icon size={20} />
            </span>
            <span>{t(`welcomeMotion.steps.${index}`)}</span>
          </button>
        ))}
      </div>
      <div className={styles.caption}>
        <p>{t(`welcomeMotion.descriptions.${phase}`)}</p>
        {!reduced && (
          <button
            type="button"
            aria-label={t("welcomeMotion.replay")}
            onClick={() => {
              setPhase(0);
              setRunning(true);
            }}
          >
            <Play size={16} />
            {t("welcomeMotion.replay")}
          </button>
        )}
      </div>
    </div>
  );
}
