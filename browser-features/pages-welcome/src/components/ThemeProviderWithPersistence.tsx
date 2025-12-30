import { ThemeProvider as BaseThemeProvider } from "@nora/ui-components";
import { getThemeSetting, setThemeSetting } from "../app/customize/dataManager";
import { useEffect, useState } from "react";

type Theme = "light" | "dark" | "system";

interface ThemeProviderWithPersistenceProps {
  children: React.ReactNode;
  defaultTheme?: Theme;
}

/**
 * Enhanced ThemeProvider with persistence using Floorp preferences.
 * Wraps the shared ThemeProvider to add theme persistence functionality.
 */
export function ThemeProviderWithPersistence({
  children,
  defaultTheme = "system",
}: ThemeProviderWithPersistenceProps) {
  const [persistedTheme, setPersistedTheme] = useState<Theme>(defaultTheme);
  const [isLoaded, setIsLoaded] = useState(false);

  // Load theme from preferences on mount
  useEffect(() => {
    const fetchTheme = async () => {
      try {
        const themeValue = await getThemeSetting();
        if (themeValue === 1) {
          setPersistedTheme("light");
        } else if (themeValue === 0) {
          setPersistedTheme("dark");
        } else {
          setPersistedTheme("system");
        }
      } catch (error) {
        console.error("Failed to get theme setting:", error);
      } finally {
        setIsLoaded(true);
      }
    };

    fetchTheme();
  }, []);

  // Don't render until theme is loaded to prevent flash
  if (!isLoaded) {
    return null;
  }

  return (
    <BaseThemeProvider defaultTheme={persistedTheme}>
      {children}
    </BaseThemeProvider>
  );
}
