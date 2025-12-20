import { type ChangeEvent, forwardRef } from "react";
import { cn } from "@/lib/utils.ts";

interface SeekbarProps {
  label?: string;
  description?: string;
  showValue?: boolean;
  showMinMax?: boolean;
  minLabel?: string;
  maxLabel?: string;
  size?: "sm" | "md" | "lg";
  valuePrefix?: string;
  valueSuffix?: string;
  min?: number;
  max?: number;
  step?: number;
  value?: number;
  disabled?: boolean;
  className?: string;
  onChange?: (e: ChangeEvent<HTMLInputElement>) => void;
  onMouseEnter?: () => void;
  onMouseLeave?: () => void;
}

const Seekbar = forwardRef<HTMLInputElement, SeekbarProps>(
  (
    {
      className,
      label,
      description,
      showValue = true,
      showMinMax = true,
      minLabel,
      maxLabel,
      size = "md",
      valuePrefix = "",
      valueSuffix = "",
      min = 0,
      max = 100,
      step,
      value = 0,
      disabled = false,
      ...props
    },
    ref,
  ) => {
    const sizeStyles = {
      sm: "h-4",
      md: "h-5",
      lg: "h-6",
    };

    return (
      <div className={cn("w-full space-y-2", className)}>
        {label && (
          <div className="mb-1">
            <div className="flex items-center justify-between">
              <label className="text-base-content/90 text-sm font-medium">
                {label}
              </label>
              {showValue && (
                <span className="text-base-content/70 text-sm font-medium">
                  {valuePrefix}
                  {value}
                  {valueSuffix}
                </span>
              )}
            </div>
            {description && (
              <p className="text-sm text-base-content/60 mt-0.5">
                {description}
              </p>
            )}
          </div>
        )}

        <input
          type="range"
          ref={ref}
          className={cn(
            "range range-primary w-full range-thumb-rounded",
            disabled && "opacity-50 cursor-not-allowed",
            sizeStyles[size],
          )}
          min={min}
          max={max}
          step={step}
          value={value}
          disabled={disabled}
          {...props}
        />

        {showMinMax && (
          <div className="flex justify-between text-xs px-1 mt-1 text-base-content/60">
            <span>{minLabel || min}</span>
            <span>{maxLabel || max}</span>
          </div>
        )}
      </div>
    );
  },
);

Seekbar.displayName = "Seekbar";

export { Seekbar };
