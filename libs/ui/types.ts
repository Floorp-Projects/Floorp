import type {
  ButtonHTMLAttributes,
  HTMLAttributes,
  InputHTMLAttributes,
  ReactNode,
  SelectHTMLAttributes,
} from "react";
export type PageTheme = "light" | "dark" | "system";
export type ResolvedPageTheme = Exclude<PageTheme, "system">;

export interface BrandProps {
  onDark?: boolean;
}

export interface ModalProps {
  title: string;
  children: ReactNode;
  onClose: () => void;
  closeLabel: string;
  closeOnEscape?: boolean;
  wide?: boolean;
}

export type SelectProps = SelectHTMLAttributes<HTMLSelectElement>;

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?:
    | "default"
    | "primary"
    | "secondary"
    | "ghost"
    | "link"
    | "light"
    | "danger";
  size?: "default" | "sm" | "lg";
  asChild?: boolean;
}

export interface DropDownOption {
  value: string;
  label: string;
  icon?: ReactNode;
}
export interface DropDownProps
  extends Omit<SelectHTMLAttributes<HTMLSelectElement>, "children"> {
  options: DropDownOption[];
}

export interface ConfirmModalProps {
  isOpen: boolean;
  onClose: () => void;
  onConfirm: () => void;
  title: string;
  children: ReactNode;
  confirmText?: string;
  cancelText?: string;
  confirmVariant?: ButtonProps["variant"];
}

export type InputProps = InputHTMLAttributes<HTMLInputElement>;

export interface SwitchProps
  extends Omit<InputHTMLAttributes<HTMLInputElement>, "size"> {
  size?: "sm" | "md" | "lg";
}

export type SectionProps = HTMLAttributes<HTMLDivElement>;

export interface SeekbarProps {
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
  onChange?: (e: import("react").ChangeEvent<HTMLInputElement>) => void;
  onMouseEnter?: () => void;
  onMouseLeave?: () => void;
}
