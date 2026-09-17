import {
  ArrowLeft,
  ArrowRight,
  Globe,
  Menu,
  Plus,
  RotateCcw,
  Shield,
  Star,
  X,
} from "lucide-react";
import type { DemoChromeProps } from "./types.ts";
import styles from "./demos.module.css";

export function DemoChrome({ title, address, action }: DemoChromeProps) {
  return (
    <div className={styles.demoChrome}>
      <div className={styles.tabStrip} aria-hidden="true">
        <span>
          <Globe size={16} />
          {title}
          <X size={14} />
        </span>
        <Plus size={18} />
        <div className={styles.windowControls}>− □ ×</div>
      </div>
      <div className={styles.toolbar}>
        <div className={styles.chromeIcons} aria-hidden="true">
          <ArrowLeft size={20} />
          <ArrowRight size={20} />
          <RotateCcw size={20} />
        </div>
        <div className={styles.urlBar}>
          <Shield size={18} aria-hidden="true" />
          <span className={styles.address}>{address}</span>
          {action}
          <Star size={18} aria-hidden="true" />
        </div>
        <Menu size={20} aria-hidden="true" />
      </div>
    </div>
  );
}
