import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

interface BreadcrumbProps {
  children?: ReactNode;
  className?: string;
}

export function Breadcrumb({ children, className }: BreadcrumbProps) {
  return (
    <nav className={cn("overflow-auto flex items-center", className)}>
      {children}
    </nav>
  );
}

export function BreadcrumbList({ children, className }: BreadcrumbProps) {
  return (
    <ol
      className={cn(
        "flex flex-wrap items-center gap-1.5 text-sm text-base-content/60",
        className,
      )}
    >
      {children}
    </ol>
  );
}

export function BreadcrumbItem({ children, className }: BreadcrumbProps) {
  return (
    <li className={cn("flex items-center gap-1.5", className)}>
      {children}
    </li>
  );
}

export function BreadcrumbPage({ children, className }: BreadcrumbProps) {
  return (
    <span
      className={cn(
        "font-medium text-base-content transition-colors hover:text-base-content/80",
        className,
      )}
    >
      {children}
    </span>
  );
}

export function BreadcrumbSeparator({ className }: { className?: string }) {
  return (
    <span
      className={cn(
        "opacity-40 select-none",
        className,
      )}
      aria-hidden="true"
    >
      /
    </span>
  );
}
