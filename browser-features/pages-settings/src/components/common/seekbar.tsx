import { type KeyboardEvent, useId } from "react";
import styles from "../../../../../libs/ui/controls.module.css";
import type { SeekbarProps } from "./types.ts";

function countDecimals(value: number): number {
  const text = String(value);
  const decimalIndex = text.indexOf(".");
  return decimalIndex === -1 ? 0 : text.length - decimalIndex - 1;
}

export function Seekbar({
  label,
  description,
  className,
  showValue = true,
  showMinMax = true,
  minLabel,
  maxLabel,
  size = "md",
  valuePrefix = "",
  valueSuffix = "",
  min = 0,
  max = 100,
  step = 1,
  value = 0,
  disabled = false,
  onValueChange,
  onMouseEnter,
  onMouseLeave,
}: SeekbarProps) {
  const id = useId();
  const descriptionId = `${id}-description`;
  const precision = Math.max(
    countDecimals(min),
    countDecimals(max),
    countDecimals(step),
  );

  const clamp = (nextValue: number): number =>
    Math.min(max, Math.max(min, Number(nextValue.toFixed(precision))));

  const updateValue = (nextValue: number): void => {
    const clampedValue = clamp(nextValue);
    if (clampedValue !== value) {
      onValueChange?.(clampedValue);
    }
  };

  const handleKeyDown = (event: KeyboardEvent<HTMLInputElement>): void => {
    const pageStep = step * 10;
    const keyActions: Record<string, number> = {
      ArrowDown: -step,
      ArrowLeft: -step,
      ArrowRight: step,
      ArrowUp: step,
      End: max - value,
      Home: min - value,
      PageDown: -pageStep,
      PageUp: pageStep,
    };
    const delta = keyActions[event.key];
    if (delta === undefined) {
      return;
    }

    event.preventDefault();
    updateValue(value + delta);
  };

  return (
    <div className={`${styles.rangeField} ${className ?? ""}`}>
      <div className={styles.rangeLabel}>
        <label htmlFor={id}>{label}</label>
        {showValue && (
          <output htmlFor={id}>{valuePrefix}{value}{valueSuffix}</output>
        )}
      </div>
      {description && (
        <p id={descriptionId} className={styles.sectionDescription}>
          {description}
        </p>
      )}
      <input
        id={id}
        type="range"
        className={styles.range}
        role="slider"
        aria-label={label}
        aria-describedby={description ? descriptionId : undefined}
        aria-valuenow={value}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuetext={`${valuePrefix}${value}${valueSuffix}`}
        min={min}
        max={max}
        step={step}
        value={value}
        disabled={disabled}
        onChange={(event) => onValueChange?.(Number(event.currentTarget.value))}
        onKeyDown={handleKeyDown}
        onMouseEnter={onMouseEnter}
        onMouseLeave={onMouseLeave}
      />
      {showMinMax && (
        <div className={styles.rangeLabel} aria-hidden="true">
          <span>{minLabel ?? min}</span>
          <span>{maxLabel ?? max}</span>
        </div>
      )}
    </div>
  );
}
