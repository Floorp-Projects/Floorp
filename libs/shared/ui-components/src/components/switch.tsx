import { forwardRef } from "react";
import { cn } from "../lib/utils";

interface SwitchProps
  extends Omit<React.InputHTMLAttributes<HTMLInputElement>, "size"> {
  size?: "sm" | "md" | "lg";
}

const Switch = forwardRef<HTMLInputElement, SwitchProps>(
  ({ className, size = "md", ...props }, ref) => {
    const sizeStyles = {
      sm: "h-4 w-7",
      md: "h-5 w-9",
      lg: "h-6 w-11",
    };

    const thumbSizeStyles = {
      sm: "h-3 w-3",
      md: "h-4 w-4",
      lg: "h-5 w-5",
    };

    const thumbTranslateStyles = {
      sm: "translate-x-3",
      md: "translate-x-4",
      lg: "translate-x-5",
    };

    return (
      <label
        className={cn(
          "relative inline-flex cursor-pointer select-none items-center",
          className,
        )}
      >
        <input
          type="checkbox"
          className="peer sr-only"
          ref={ref}
          {...props}
        />
        <span
          className={cn(
            "flex items-center rounded-full p-0.5 duration-200",
            "bg-base-300 dark:bg-base-700",
            "peer-checked:bg-primary",
            "peer-focus-visible:ring-2 peer-focus-visible:ring-offset-2 peer-focus-visible:ring-primary/20",
            "peer-disabled:cursor-not-allowed peer-disabled:opacity-50",
            sizeStyles[size],
          )}
        >
          <span
            className={cn(
              "aspect-square rounded-full bg-white",
              "transition-all duration-200",
              "dark:bg-base-800",
              {
                "translate-x-0": !props.checked,
                [thumbTranslateStyles[size]]: props.checked,
              },
              thumbSizeStyles[size],
            )}
          />
        </span>
      </label>
    );
  },
);

Switch.displayName = "Switch";

export { Switch };
