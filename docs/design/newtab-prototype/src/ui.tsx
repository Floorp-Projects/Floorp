import React from "react";
import type {
  ButtonProps,
  EmptyStateProps,
  IconButtonProps,
} from "./ui-types.ts";
import { Box, BookOpen, Globe, Rss } from "lucide-react";
import {
  SiGithub,
  SiMdnwebdocs,
  SiDeno,
  SiReact,
  SiTypescript,
  SiNotion,
  SiWikipedia,
  SiNpm,
} from "@icons-pack/react-simple-icons";
import lightLogo from "../../../../libs/ui/assets/Floorp_Logo_B_Light.svg";
import darkLogo from "../../../../libs/ui/assets/Floorp_Logo_B_Dark.svg";
import floorpIcon from "../../../../static/gecko/custom-app-icons/floorp-simple.png";
import { Button as SharedButton } from "../../../../libs/ui/button.tsx";
export { Input } from "../../../../libs/ui/input.tsx";

export function Logo({
  onDark = false,
  className = "",
}: {
  onDark?: boolean;
  className?: string;
}) {
  return (
    <img
      className={`floorp-logo ${className}`}
      src={onDark ? darkLogo : lightLogo}
      alt="Floorp"
    />
  );
}

export function BrandIcon({
  name,
  size = 32,
}: {
  name: string;
  size?: number;
}) {
  const props = { size, "aria-hidden": true as const };
  switch (name) {
    case "floorp":
      return <img src={floorpIcon} width={size} height={size} alt="" />;
    case "github":
      return <SiGithub {...props} />;
    case "mdn":
      return <SiMdnwebdocs {...props} />;
    case "deno":
      return <SiDeno {...props} />;
    case "react":
      return <SiReact {...props} color="#00a7d6" />;
    case "typescript":
      return <SiTypescript {...props} color="#3178c6" />;
    case "figma":
      return (
        <svg width={size} height={size} viewBox="0 0 38 57" aria-hidden="true">
          <path d="M9.5 0H19v19H9.5a9.5 9.5 0 0 1 0-19Z" fill="#f24e1e" />
          <path d="M19 0h9.5a9.5 9.5 0 0 1 0 19H19Z" fill="#ff7262" />
          <path d="M9.5 19H19v19H9.5a9.5 9.5 0 0 1 0-19Z" fill="#a259ff" />
          <circle cx="28.5" cy="28.5" r="9.5" fill="#1abcfe" />
          <path d="M9.5 38H19v9.5A9.5 9.5 0 1 1 9.5 38Z" fill="#0acf83" />
        </svg>
      );
    case "notion":
      return <SiNotion {...props} />;
    case "wikipedia":
      return <SiWikipedia {...props} />;
    case "npm":
      return <SiNpm {...props} color="#cb3837" />;
    case "cube":
      return (
        <Box
          {...props}
          color="var(--floorp-subtle)"
          fill="var(--floorp-brand)"
          strokeWidth={1.3}
        />
      );
    case "book":
      return <BookOpen {...props} />;
    case "rss":
      return <Rss {...props} />;
    default:
      return <Globe {...props} />;
  }
}

export function IconButton({
  label,
  children,
  className = "",
  type = "button",
  ...props
}: IconButtonProps) {
  return (
    <SharedButton
      type={type}
      variant="ghost"
      aria-label={label}
      title={label}
      className={`icon-button ${className}`}
      {...props}
    >
      {children}
    </SharedButton>
  );
}

export function Button({
  children,
  variant = "primary",
  className = "",
  type = "button",
  ...props
}: ButtonProps) {
  return (
    <SharedButton
      type={type}
      variant={variant}
      className={`ui-button ui-button-${variant} ${className}`}
      {...props}
    >
      {children}
    </SharedButton>
  );
}

export function EmptyState({ title, description, children }: EmptyStateProps) {
  return (
    <div className="empty-state">
      <BookOpen size={30} strokeWidth={1.4} aria-hidden="true" />
      <h3>{title}</h3>
      {description && <p>{description}</p>}
      {children}
    </div>
  );
}
