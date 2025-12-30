import { forwardRef, type ChangeEvent } from "react";
import { cn } from "../lib/utils";

interface SeekbarProps {
    label?: string;
    showValue?: boolean;
    showMinMax?: boolean;
    minLabel?: string;
    maxLabel?: string;
    size?: "sm" | "md" | "lg";
    valuePrefix?: string;
    valueSuffix?: string;
    min?: number;
    max?: number;
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
            showValue = true,
            showMinMax = true,
            minLabel,
            maxLabel,
            size = "md",
            valuePrefix = "",
            valueSuffix = "",
            min = 0,
            max = 100,
            value = 0,
            disabled = false,
            ...props
        },
        ref,
    ) => {
        const sizeStyles = {
            sm: "h-1",
            md: "h-2",
            lg: "h-3",
        };

        return (
            <div className={cn("w-full space-y-2", className)}>
                {label && (
                    <div className="flex items-center justify-between mb-1">
                        <label className="text-base-content/90 text-sm">{label}</label>
                        {showValue && (
                            <span className="text-base-content/70 text-sm font-medium">
                                {valuePrefix}
                                {value}
                                {valueSuffix}
                            </span>
                        )}
                    </div>
                )}

                <input
                    type="range"
                    ref={ref}
                    className={cn(
                        "range range-primary w-full",
                        disabled && "opacity-50 cursor-not-allowed",
                        sizeStyles[size],
                    )}
                    min={min}
                    max={max}
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
