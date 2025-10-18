import { useState } from "react";
import { Background } from "./components/Background/index.tsx";
import { BackgroundProvider } from "./contexts/BackgroundContext.tsx";
import { ComponentsProvider } from "./contexts/ComponentsContext.tsx";
import { Settings } from "./components/Settings/index.tsx";
import { SettingsButton } from "./components/SettingsButton/index.tsx";
import { DefaultLayout } from "./components/DefaultLayout.tsx";
import { FirefoxNewTabLayout } from "./components/FirefoxNewTabLayout.tsx";
import "./globals.css";
import { useComponents } from "./contexts/ComponentsContext.tsx";

function NewTabContent() {
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const { components } = useComponents();

  return (
    <>
      <Background />
      <div className="relative w-full min-h-screen">
        {components.firefoxLayout ? <FirefoxNewTabLayout /> : <DefaultLayout />}
        <div className="fixed bottom-4 right-4">
          <SettingsButton onClick={() => setIsSettingsOpen(true)} />
          <Settings
            isOpen={isSettingsOpen}
            onClose={() => setIsSettingsOpen(false)}
          />
        </div>
      </div>
    </>
  );
}

export default function App() {
  return (
    <ComponentsProvider>
      <BackgroundProvider>
        <NewTabContent />
      </BackgroundProvider>
    </ComponentsProvider>
  );
}
