import { ThemeProvider as BaseThemeProvider } from "@nora/ui-components";
import { getThemeSetting, setThemeSetting } from "../app/customize/dataManager";
import { useEffect, useState } from "react";

type Theme = "light" | "dark" | "system";

interface ThemeProviderWithPersistenceProps {
  children: React.ReactNode;
  defaultTheme?: Theme;
}

// Floorp preference values for theme
const THEME_PREF_DARK = 0;
const THEME_PREF_LIGHT = 1;
const THEME_PREF_SYSTEM = 2;

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
        if (themeValue === THEME_PREF_LIGHT) {
          setPersistedTheme("light");
        } else if (themeValue === THEME_PREF_DARK) {
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
