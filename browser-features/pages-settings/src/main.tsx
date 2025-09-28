import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { MemoryRouter } from "react-router-dom";
import { ThemeProvider } from "@/components/theme-provider.tsx";
import "@/lib/i18n/i18n.ts";

const getInitialEntry = () => {
  const hash = globalThis.location.hash.slice(1);
  return hash && hash.startsWith("/") ? hash : "/overview/home";
};

globalThis.addEventListener("hashchange", () => {
  if (document.documentElement.dataset.isRouteChanged === "true") {
    document.documentElement.dataset.isRouteChanged = "false";
    return;
  }

  const hash = globalThis.location.hash.slice(1);
  if (hash && hash.startsWith("/")) {
    globalThis.location.reload();
  }
});

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ThemeProvider defaultTheme="system">
      <MemoryRouter initialEntries={[getInitialEntry()]}>
        <App />
      </MemoryRouter>
    </ThemeProvider>
  </StrictMode>,
);
