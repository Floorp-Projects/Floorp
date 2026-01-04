import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { MemoryRouter } from "react-router-dom";
import { ThemeProvider } from "@nora/ui-components";
import { I18nProvider } from "@/lib/i18n/I18nProvider.tsx";

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
      <I18nProvider>
        <MemoryRouter initialEntries={[getInitialEntry()]}>
          <App />
        </MemoryRouter>
      </I18nProvider>
    </ThemeProvider>
  </StrictMode>,
);
