// SPDX-License-Identifier: MPL-2.0
import type {
  FloatingMenuProps,
  SiteIconProps,
  ModalProps,
  IconButtonProps,
} from "./types.ts";
import { forwardRef, useEffect, useRef } from "react";
import { createPortal } from "react-dom";
import { Folder, Globe, X } from "lucide-react";
import {
  SiGithub,
  SiMdnwebdocs,
  SiDeno,
  SiReact,
  SiTypescript,
  SiWikipedia,
  SiNotion,
  SiNpm,
} from "@icons-pack/react-simple-icons";
import { Button } from "../../../../../libs/ui/button.tsx";
import floorpIcon from "../../../../../static/gecko/custom-app-icons/floorp-simple.png";
export { Button };
export const IconButton = forwardRef<HTMLButtonElement, IconButtonProps>(
  function IconButton({ label, children, className = "", ...props }, ref) {
    return (
      <Button
        ref={ref}
        type="button"
        variant="ghost"
        className={`nt-icon-button ${className}`}
        aria-label={label}
        title={label}
        {...props}
      >
        {children}
      </Button>
    );
  },
);
export function FloatingMenu({
  anchor,
  close,
  label,
  children,
}: FloatingMenuProps) {
  const ref = useRef<HTMLDivElement>(null);
  const rect = anchor.current?.getBoundingClientRect();
  const width = Math.min(244, innerWidth - 32);
  useEffect(() => {
    ref.current
      ?.querySelector<HTMLButtonElement>("button:not(:disabled)")
      ?.focus({ preventScroll: true });
  }, []);
  useEffect(() => {
    const pointer = (event: PointerEvent) => {
      if (
        !ref.current?.contains(event.target as Node) &&
        !anchor.current?.contains(event.target as Node)
      )
        close();
    };
    const key = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        event.preventDefault();
        close();
        anchor.current?.focus();
      } else if (
        ["ArrowDown", "ArrowUp", "Home", "End"].includes(event.key) &&
        ref.current?.contains(document.activeElement)
      ) {
        const buttons = Array.from(
          ref.current.querySelectorAll<HTMLButtonElement>(
            "button:not(:disabled)",
          ),
        );
        if (!buttons.length) return;
        const index = buttons.indexOf(
          document.activeElement as HTMLButtonElement,
        );
        const next =
          event.key === "Home"
            ? 0
            : event.key === "End"
              ? buttons.length - 1
              : (index +
                  (event.key === "ArrowDown" ? 1 : -1) +
                  buttons.length) %
                buttons.length;
        event.preventDefault();
        buttons[next].focus({ preventScroll: true });
      }
    };
    const focus = (event: FocusEvent) => {
      if (
        !ref.current?.contains(event.target as Node) &&
        !anchor.current?.contains(event.target as Node)
      )
        close();
    };
    const reposition = () => close();
    addEventListener("pointerdown", pointer);
    addEventListener("keydown", key);
    addEventListener("focusin", focus);
    addEventListener("resize", reposition);
    addEventListener("scroll", reposition, true);
    return () => {
      removeEventListener("pointerdown", pointer);
      removeEventListener("keydown", key);
      removeEventListener("focusin", focus);
      removeEventListener("resize", reposition);
      removeEventListener("scroll", reposition, true);
    };
  }, [anchor, close]);
  return createPortal(
    <div
      ref={ref}
      className="nt-tile-menu nt-floating-menu"
      role="group"
      aria-label={label}
      style={{
        width,
        left: Math.min(
          Math.max(16, (rect?.right ?? 16) - width),
          innerWidth - width - 16,
        ),
        top: Math.min(
          Math.max(16, rect?.bottom ?? 16),
          Math.max(16, innerHeight - 256),
        ),
      }}
    >
      {children}
    </div>,
    document.body,
  );
}
export function SiteIcon({ url, folder = false }: SiteIconProps) {
  if (folder)
    return (
      <Folder
        className="nt-folder-icon"
        size={44}
        strokeWidth={1.5}
        aria-hidden="true"
      />
    );
  let hostname = "";
  try {
    hostname = new URL(url ?? "").hostname.replace(/^www\./, "");
  } catch {
    /* A legacy unsupported URL keeps a neutral icon. */
  }
  const props = { size: 44, "aria-hidden": true as const };
  if (hostname === "github.com") return <SiGithub {...props} />;
  if (hostname === "floorp.app" || hostname.endsWith(".floorp.app"))
    return <img src={floorpIcon} width={44} height={44} alt="" />;
  if (hostname === "developer.mozilla.org") return <SiMdnwebdocs {...props} />;
  if (hostname === "deno.com" || hostname === "deno.land")
    return <SiDeno {...props} />;
  if (hostname === "react.dev") return <SiReact {...props} color="#00a7d6" />;
  if (hostname === "typescriptlang.org")
    return <SiTypescript {...props} color="#3178c6" />;
  if (hostname === "wikipedia.org" || hostname.endsWith(".wikipedia.org"))
    return <SiWikipedia {...props} />;
  if (hostname === "notion.so") return <SiNotion {...props} />;
  if (hostname === "npmjs.com") return <SiNpm {...props} color="#cb3837" />;
  return <Globe {...props} strokeWidth={1.5} />;
}
export function domain(url?: string | null): string {
  try {
    return new URL(url ?? "").hostname.replace(/^www\./, "");
  } catch {
    return "";
  }
}
export function safeWebUrl(value?: string | null): string | null {
  try {
    const url = new URL(value ?? "");
    return ["http:", "https:"].includes(url.protocol) &&
      !url.username &&
      !url.password
      ? url.href
      : null;
  } catch {
    return null;
  }
}
export function Modal({ title, closeLabel, close, children }: ModalProps) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const dialog = ref.current;
    const previous = document.activeElement;
    dialog?.showModal();
    const target = dialog?.querySelector<HTMLElement>(
      "[data-initial-focus], input, select, button[data-cancel]",
    );
    target?.focus();
    return () => {
      dialog?.close();
      if (previous instanceof HTMLElement && previous.isConnected)
        previous.focus();
      else
        requestAnimationFrame(() => {
          const main =
            document.getElementById("dial-location") ??
            document.getElementById("main-content");
          if (main) {
            main.tabIndex = -1;
            main.focus();
          }
        });
    };
  }, []);
  return (
    <dialog
      ref={ref}
      className="nt-dialog"
      aria-labelledby="nt-dialog-title"
      onCancel={(event) => {
        event.preventDefault();
        close();
      }}
      onClick={(event) => {
        if (event.target === event.currentTarget) close();
      }}
    >
      <header className="nt-dialog-header">
        <h2 id="nt-dialog-title">{title}</h2>
        <IconButton label={closeLabel} onClick={close}>
          <X size={20} />
        </IconButton>
      </header>
      {children}
    </dialog>
  );
}
