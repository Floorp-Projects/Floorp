import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

interface CardProps {
  children: ReactNode;
  className?: string;
}

export function Card({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "flex flex-col bg-base-200 shadow-md border rounded border-base-300/20 dark:bg-base-600 mb-8 z-10",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function CardHeader({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "p-4 border-base-300/20",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function CardTitle({ children, className }: CardProps) {
  return (
    <h2
      className={cn("card-title text-base-content/90 font-medium", className)}
    >
      {children}
    </h2>
  );
}

export function CardDescription({ children, className }: CardProps) {
  return (
    <p className={cn("text-base-content/60 text-sm", className)}>
      {children}
    </p>
  );
}

export function CardContent({ children, className }: CardProps) {
  return (
    <div className={cn("card-body pt-0 pb-2 px-4 mb-2", className)}>
      {children}
    </div>
  );
}

export function CardFooter({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "flex card-body pt-0 px-4 border-t border-base-300/20 bg-base-200/30 dark:bg-base-200/10 justify-end",
        className,
      )}
      role="contentinfo"
      aria-label="Card footer"
    >
      {children}
    </div>
  );
}
