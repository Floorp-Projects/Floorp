import { forwardRef, type SelectHTMLAttributes, type ReactNode } from "react";
import { cn } from "@/lib/utils";

export interface DropDownOption {
  value: string;
  label: string;
  icon?: ReactNode;
}

export interface DropDownProps extends Omit<SelectHTMLAttributes<HTMLSelectElement>, 'children'> {
  options: DropDownOption[];
}

const DropDown = forwardRef<HTMLSelectElement, DropDownProps>(
  ({ className, options, ...props }, ref) => {
    // Check if any option has an icon
    const hasIcons = options.some(option => option.icon);

    return (
      <div className="relative w-full">
        {/* Icon display (shown only in light mode, hidden in select) */}
        {hasIcons && (
          <div className="absolute left-3 top-1/2 -translate-y-1/2 pointer-events-none z-10 flex items-center">
            {options.find(opt => opt.value === props.value)?.icon}
          </div>
        )}

        <select
          className={cn(
            "flex h-10 w-full rounded-md border border-base-content/30 bg-base-100 text-base-content",
            "py-2 text-sm",
            hasIcons ? "pl-10 pr-10" : "px-3 pr-10",
            "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary/50",
            "disabled:cursor-not-allowed disabled:opacity-50",
            "appearance-none cursor-pointer",
            "transition-colors",
            // Light mode dropdown arrow
            "bg-[url('data:image/svg+xml;charset=utf-8,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%2212%22%20height%3D%228%22%20viewBox%3D%220%200%2012%208%22%3E%3Cpath%20fill%3D%22%23666%22%20d%3D%22M6%208L0%202%201.4.6%206%205.2%2010.6.6%2012%202z%22%2F%3E%3C%2Fsvg%3E')]",
            // Dark mode dropdown arrow
            "dark:bg-[url('data:image/svg+xml;charset=utf-8,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%2212%22%20height%3D%228%22%20viewBox%3D%220%200%2012%208%22%3E%3Cpath%20fill%3D%22%23ccc%22%20d%3D%22M6%208L0%202%201.4.6%206%205.2%2010.6.6%2012%202z%22%2F%3E%3C%2Fsvg%3E')]",
            "bg-size-[12px_8px] bg-position-[right_12px_center] bg-no-repeat",
            // Hover states
            "hover:border-base-content/50",
            className,
          )}
          ref={ref}
          {...props}
        >
          {options.map((option) => (
            <option key={option.value} value={option.value}>
              {option.label}
            </option>
          ))}
        </select>
      </div>
    );
  },
);
DropDown.displayName = "DropDown";

export { DropDown };
