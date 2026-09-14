export type { SwitchProps } from "./types.ts";
import type { SwitchProps } from "./types.ts";
import { forwardRef } from "react";
import styles from "./controls.module.css";

// Native input contract intentionally preserves RHF refs and change events.
// CSS :checked supports controlled and uncontrolled callers equally.
export const Switch = forwardRef<HTMLInputElement, SwitchProps>(function Switch(
  { className = "", size: _size, ...props },
  ref,
) {
  return (
    <span className={`${styles.switch} ${className}`}>
      <input
        {...props}
        ref={ref}
        type="checkbox"
        role="switch"
        className={styles.switchInput}
      />
      <span className={styles.track} aria-hidden="true">
        <span className={styles.thumb} />
      </span>
    </span>
  );
});
