export type { ButtonProps } from "./types.ts";
import type { ButtonProps } from "./types.ts";
import { Button as ChakraButton } from "@chakra-ui/react";
import { forwardRef } from "react";
import styles from "./controls.module.css";

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(
  function Button(
    {
      variant = "primary",
      size = "default",
      className = "",
      asChild,
      type = "button",
      ...props
    },
    ref,
  ) {
    return (
      <ChakraButton
        unstyled
        asChild={asChild}
        ref={ref}
        type={type}
        className={`${styles.button} ${className}`}
        data-variant={variant}
        data-size={size}
        {...props}
      />
    );
  },
);
