export type { DropDownOption, DropDownProps } from "./types.ts";
import type { DropDownOption, DropDownProps } from "./types.ts";
import { forwardRef } from "react";
import styles from "./controls.module.css";
import type { SelectProps } from "./types.ts";

export const Select = forwardRef<HTMLSelectElement, SelectProps>(
  function Select({ className = "", ...props }, ref) {
    return (
      <select
        {...props}
        ref={ref}
        className={`${styles.select} ${className}`}
      />
    );
  },
);

export const DropDown = forwardRef<HTMLSelectElement, DropDownProps>(
  function DropDown({ options, className = "", ...props }, ref) {
    const icon = options.find((option) => option.value === props.value)?.icon;
    return (
      <div className={styles.selectWrapper}>
        {icon && (
          <span className={styles.selectIcon} aria-hidden="true">{icon}</span>
        )}
        <select
          {...props}
          ref={ref}
          className={`${styles.select} ${className}`}
          data-has-icon={!!icon}
        >
          {options.map(({ value, label }) => (
            <option key={value} value={value}>{label}</option>
          ))}
        </select>
      </div>
    );
  },
);
