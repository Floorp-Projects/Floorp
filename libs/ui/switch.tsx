export type { SwitchProps } from "./types.ts";
import type { SwitchProps } from "./types.ts";
import { forwardRef, useState } from "react";
import styles from "./controls.module.css";
import { Switch as ChakraSwitch } from "@chakra-ui/react";
import { useStandardControls } from "./control-theme.ts";

// Keep the native checkbox event contract used by settings forms. Ark's hidden
// input stops click propagation, which suppresses React change events in Gecko.
const StandardSwitch = forwardRef<HTMLInputElement, SwitchProps>(
  function StandardSwitch({ className = "", size = "md", ...props }, ref) {
    const [checked, setChecked] = useState(props.defaultChecked ?? false);
    return (
      <ChakraSwitch.Root
        as="span"
        checked={props.checked ?? checked}
        disabled={props.disabled}
        readOnly={props.readOnly}
        className={`${styles.standardSwitch} ${className}`}
        colorPalette="purple"
        size={size}
        flexShrink={0}
        minH="10"
      >
        <input
          {...props}
          ref={ref}
          type="checkbox"
          role="switch"
          className={styles.switchInput}
          onChange={(event) => {
            setChecked(event.currentTarget.checked);
            props.onChange?.(event);
          }}
        />
        <ChakraSwitch.Control>
          <ChakraSwitch.Thumb />
        </ChakraSwitch.Control>
      </ChakraSwitch.Root>
    );
  },
);

export const Switch = forwardRef<HTMLInputElement, SwitchProps>(function Switch(
  { className = "", size: _size, ...props },
  ref,
) {
  const standard = useStandardControls();
  if (standard) {
    return (
      <StandardSwitch
        {...props}
        ref={ref}
        className={className}
        size={_size}
      />
    );
  }
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
