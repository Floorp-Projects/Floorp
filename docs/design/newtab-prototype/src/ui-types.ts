import type { ButtonHTMLAttributes, PropsWithChildren } from "react";

export interface IconButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  label: string;
}
export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "primary" | "secondary" | "ghost";
}
export type EmptyStateProps = PropsWithChildren<{
  title: string;
  description?: string;
}>;
