import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { ThemeProvider } from "@/components/theme-provider.tsx";
import { I18nProvider } from "@/lib/i18n/I18nProvider.tsx";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ThemeProvider>
      <I18nProvider>
        <App />
      </I18nProvider>
    </ThemeProvider>
  </StrictMode>,
);
