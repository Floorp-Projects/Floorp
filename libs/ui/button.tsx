export type { ButtonProps } from "./types.ts";
import type { ButtonProps } from "./types.ts";
import { Button as ChakraButton } from "@chakra-ui/react";
import { forwardRef } from "react";
import styles from "./controls.module.css";
import { useStandardControls } from "./control-theme.ts";

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
    const standard = useStandardControls();
    if (standard) {
      return (
        <ChakraButton
          asChild={asChild}
          ref={ref}
          type={type}
          colorPalette={variant === "danger" ? "red" : "purple"}
          variant={variant === "ghost"
            ? "ghost"
            : variant === "link"
            ? "plain"
            : variant === "secondary" || variant === "light"
            ? "outline"
            : "solid"}
          size={size === "default" ? "md" : size}
          className={className}
          whiteSpace="normal"
          flexShrink={0}
          {...props}
        />
      );
    }
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
