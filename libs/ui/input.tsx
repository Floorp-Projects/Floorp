export type { InputProps } from "./types.ts";
import type { InputProps } from "./types.ts";
import { Input as ChakraInput } from "@chakra-ui/react";
import { forwardRef } from "react";
import styles from "./controls.module.css";

export const Input = forwardRef<HTMLInputElement, InputProps>(
  function Input({ className = "", size, ...props }, ref) {
    return (
      <ChakraInput
        unstyled
        htmlSize={size}
        ref={ref}
        className={`${styles.input} ${className}`}
        {...props}
      />
    );
  },
);
