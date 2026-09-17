import { ArrowLeft, Briefcase, House, Moon, Mouse } from "lucide-react";
import styles from "./home.module.css";

export function SettingPreview({ kind }: { kind: string }) {
  const tabs = (
    <div className={styles.miniTabs}>
      <i />
      <i />
      <i />
    </div>
  );
  return (
    <div className={styles.preview} data-kind={kind} aria-hidden="true">
      {kind === "design" && (
        <div className={styles.miniBrowser}>
          {tabs}
          <div className={styles.swatches}>
            <i />
            <i />
            <i />
          </div>
        </div>
      )}
      {kind === "workspaces" && (
        <div className={styles.workspaceStack}>
          <div className={styles.miniBrowser}>
            <Briefcase size={18} />
            {tabs}
          </div>
          <div className={styles.miniBrowser}>
            <House size={18} />
            {tabs}
          </div>
        </div>
      )}
      {kind === "sidebar" && (
        <div className={styles.miniBrowser}>
          {tabs}
          <div className={styles.miniSplit}>
            <aside>
              <i />
              <i />
              <i />
            </aside>
            <div>
              <b />
              <i />
              <i />
            </div>
          </div>
        </div>
      )}
      {kind === "gesture" && (
        <div className={styles.gesture}>
          <ArrowLeft size={62} strokeWidth={2} />
          <Mouse size={64} strokeWidth={1.4} />
        </div>
      )}
      {kind === "shortcuts" && (
        <div className={styles.keycaps}>
          <span>Ctrl</span>
          <span>K</span>
          <span>↵</span>
        </div>
      )}
      {kind === "performance" && (
        <div className={styles.sleep}>
          <Moon size={28} />
          <div className={styles.miniBrowser}>
            {tabs}
            <div className={styles.sleepLines}>
              <i />
              <i />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
