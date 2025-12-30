import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { ThemeProviderWithPersistence } from "./components/ThemeProviderWithPersistence.tsx";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ThemeProviderWithPersistence>
      <App />
    </ThemeProviderWithPersistence>
  </StrictMode>,
);
