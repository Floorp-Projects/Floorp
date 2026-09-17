import {
  createContext,
  type ReactNode,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";
import { useLocation } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { Menu, X } from "lucide-react";
import { Button } from "../../../../../libs/ui/button.tsx";
import styles from "./sidebar.module.css";
const SidebarContext = createContext({
  isMobile: false,
  open: false,
  setOpen: (_open: boolean) => {},
});
export const useSidebar = () => useContext(SidebarContext);
export function SidebarProvider({ children }: { children: ReactNode }) {
  const [open, setOpen] = useState(false);
  const [isMobile, setMobile] = useState(() =>
    globalThis.matchMedia("(max-width: 1100px)").matches
  );
  const { pathname } = useLocation();
  useEffect(() => {
    setOpen(false);
  }, [pathname]);
  useEffect(() => {
    const media = globalThis.matchMedia("(max-width: 1100px)");
    const update = () => setMobile(media.matches);
    media.addEventListener("change", update);
    return () => media.removeEventListener("change", update);
  }, []);
  return (
    <SidebarContext.Provider value={{ isMobile, open, setOpen }}>
      {children}
    </SidebarContext.Provider>
  );
}
interface SidebarProps {
  children?: ReactNode;
  className?: string;
  collapsible?: "icon";
}
export function Sidebar({ children, className = "" }: SidebarProps) {
  const { isMobile, open, setOpen } = useSidebar();
  const { t } = useTranslation();
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    if (isMobile && open) ref.current?.showModal();
    else ref.current?.close();
  }, [isMobile, open]);
  if (!isMobile) {
    return (
      <aside className={`${styles.sidebar} ${className}`}>
        <nav
          aria-label={t("ui.settingsNavigation", {
            defaultValue: "Settings navigation",
          })}
        >
          {children}
        </nav>
      </aside>
    );
  }
  return (
    <dialog
      ref={ref}
      className={styles.dialog}
      onClose={() => setOpen(false)}
      aria-label={t("ui.settingsNavigation", {
        defaultValue: "Settings navigation",
      })}
    >
      <Button
        variant="ghost"
        onClick={() => setOpen(false)}
        aria-label={t("ui.closeNavigation", {
          defaultValue: "Close navigation",
        })}
      >
        <X size={20} />
      </Button>
      <nav>{children}</nav>
    </dialog>
  );
}
export function SidebarHeader({ children }: SidebarProps) {
  return <div className={styles.header}>{children}</div>;
}
export function SidebarContent({ children }: SidebarProps) {
  return <div className={styles.content}>{children}</div>;
}
export function SidebarGroup({ children }: SidebarProps) {
  return <div className={styles.group}>{children}</div>;
}
export function SidebarGroupLabel({ children }: SidebarProps) {
  return <h2 className={styles.label}>{children}</h2>;
}
export function SidebarMenu({ children }: SidebarProps) {
  return <div>{children}</div>;
}
export function SidebarTrigger() {
  const { isMobile, open, setOpen } = useSidebar();
  const { t } = useTranslation();
  return isMobile
    ? (
      <Button
        variant="ghost"
        aria-expanded={open}
        aria-label={t("ui.openNavigation", { defaultValue: "Open navigation" })}
        onClick={() => setOpen(true)}
      >
        <Menu size={20} />
      </Button>
    )
    : null;
}
export function SidebarRail() {
  return null;
}
// Retain the existing exports for feature modules migrated separately.
export function SidebarFooter({ children, className }: SidebarProps) {
  return <div className={className}>{children}</div>;
}
export function SidebarMenuItem(
  { children, className }: SidebarProps & { href?: string },
) {
  return <div className={className}>{children}</div>;
}
export function SidebarMenuButton(
  { children, className, asChild }: SidebarProps & { asChild?: boolean },
) {
  return asChild
    ? <span className={className}>{children}</span>
    : <button type="button" className={className}>{children}</button>;
}
export function SidebarMenuAction(
  { children, className }: SidebarProps & { showOnHover?: boolean },
) {
  return <button type="button" className={className}>{children}</button>;
}
