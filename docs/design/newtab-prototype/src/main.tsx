import React from "react";
import { createRoot } from "react-dom/client";
import { StandardUIProvider } from "../../../../libs/ui/standard-provider.tsx";
import { PrototypeProvider } from "./store.tsx";
import { App } from "./App.tsx";
import "../../../../libs/ui/page.css";
import "./styles.css";
import "./translucency.css";

createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <StandardUIProvider>
      <PrototypeProvider>
        <App />
      </PrototypeProvider>
    </StandardUIProvider>
  </React.StrictMode>,
);
