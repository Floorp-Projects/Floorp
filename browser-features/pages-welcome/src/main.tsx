import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { ThemeProvider } from "@/components/theme-provider.tsx";
import { FloorpUIProvider } from "../../../libs/ui/provider.tsx";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <FloorpUIProvider>
      <ThemeProvider>
        <App />
      </ThemeProvider>
    </FloorpUIProvider>
  </StrictMode>,
);
