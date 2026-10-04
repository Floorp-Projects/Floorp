export type { DropDownOption, DropDownProps } from "./types.ts";
import type { DropDownProps } from "./types.ts";
import { forwardRef } from "react";
import styles from "./controls.module.css";
import type { SelectProps } from "./types.ts";
import { NativeSelect } from "@chakra-ui/react";
import { useStandardControls } from "./control-theme.ts";

export const Select = forwardRef<HTMLSelectElement, SelectProps>(
  function Select({ className = "", ...props }, ref) {
    const standard = useStandardControls();
    if (standard && !props.multiple && !props.size) {
      return (
        <NativeSelect.Root
          className={className}
          disabled={props.disabled}
          colorPalette="purple"
          minW="0"
        >
          <NativeSelect.Field {...props} ref={ref} />
          <NativeSelect.Indicator />
        </NativeSelect.Root>
      );
    }
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
    const standard = useStandardControls();
    const icon = options.find((option) => option.value === props.value)?.icon;
    if (standard) {
      return (
        <div className={`${styles.selectWrapper} ${className}`}>
          {icon && (
            <span className={styles.selectIcon} aria-hidden="true">{icon}</span>
          )}
          <NativeSelect.Root
            disabled={props.disabled}
            colorPalette="purple"
            minW="0"
          >
            <NativeSelect.Field
              {...props}
              ref={ref}
              paddingInlineStart={icon ? "10" : undefined}
            >
              {options.map(({ value, label }) => (
                <option key={value} value={value}>{label}</option>
              ))}
            </NativeSelect.Field>
            <NativeSelect.Indicator />
          </NativeSelect.Root>
        </div>
      );
    }
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
