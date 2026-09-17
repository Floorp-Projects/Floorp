import { forwardRef, useId } from "react";
import type { SeekbarProps } from "./types.ts";
import styles from "./controls.module.css";

export const Seekbar = forwardRef<HTMLInputElement, SeekbarProps>(function Seekbar(
  { label, description, className = "", showValue = true, showMinMax = true, minLabel, maxLabel, size: _size, valuePrefix = "", valueSuffix = "", min = 0, max = 100, step, value = 0, disabled = false, ...props }, ref,
) {
  const id = useId();
  return <div className={`${styles.rangeField} ${className}`}>
    <div className={styles.rangeLabel}>
      <label htmlFor={id}>{label}</label>
      {showValue && <output htmlFor={id}>{valuePrefix}{value}{valueSuffix}</output>}
    </div>
    {description && <p id={`${id}-description`} className={styles.sectionDescription}>{description}</p>}
    <input {...props} id={id} ref={ref} type="range" className={styles.range} min={min} max={max} step={step} value={value} disabled={disabled} aria-describedby={description ? `${id}-description` : undefined} aria-valuetext={`${valuePrefix}${value}${valueSuffix}`} />
    {showMinMax && <div className={styles.rangeLabel} aria-hidden="true"><span>{minLabel ?? min}</span><span>{maxLabel ?? max}</span></div>}
  </div>;
});
