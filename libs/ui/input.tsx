export type { InputProps } from "./types.ts";
import type { InputProps } from "./types.ts";
import { Input as ChakraInput } from "@chakra-ui/react";
import { forwardRef } from "react";
import styles from "./controls.module.css";
import { useStandardControls } from "./control-theme.ts";

export const Input = forwardRef<HTMLInputElement, InputProps>(
  function Input({ className = "", size, ...props }, ref) {
    const standard = useStandardControls();
    return (
      <ChakraInput
        unstyled={!standard}
        colorPalette={standard ? "purple" : undefined}
        htmlSize={size}
        ref={ref}
        className={standard ? className : `${styles.input} ${className}`}
        {...props}
      />
    );
  },
);
