import { createContext, useContext, useEffect, useRef, useState } from "react";
import { usePageTheme } from "../../../../libs/ui/use-page-theme.ts";
import { themeFromPreference } from "../../../../libs/ui/theme-value.ts";
import { getThemeSetting, setThemeSetting } from "../app/customize/dataManager";

type Theme = "light" | "dark" | "system";

type ThemeProviderProps = {
  children: React.ReactNode;
  defaultTheme?: Theme;
  storageKey?: string;
};

type ThemeProviderState = {
  theme: Theme;
  setTheme: (theme: Theme) => Promise<void>;
};

const initialState: ThemeProviderState = {
  theme: "system",
  setTheme: async () => {},
};

const ThemeProviderContext = createContext<ThemeProviderState>(initialState);

export function ThemeProvider({
  children,
  defaultTheme = "system",
  storageKey: _storageKey = "floorp-ui-theme",
  ...props
}: ThemeProviderProps) {
  const [theme, setTheme] = useState<Theme>(defaultTheme);
  const userChanged = useRef(false);
  usePageTheme(theme);

  useEffect(() => {
    let active = true;
    const fetchTheme = async () => {
      try {
        const themeValue = await getThemeSetting();
        if (active && !userChanged.current) {
          setTheme(themeFromPreference(themeValue));
        }
      } catch (error) {
        console.error("テーマ設定の取得に失敗しました:", error);
      }
    };

    fetchTheme();
    return () => {
      active = false;
    };
  }, []);

  const value = {
    theme,
    setTheme: async (newTheme: Theme) => {
      userChanged.current = true;
      await setThemeSetting(newTheme);
      setTheme(newTheme);
    },
  };

  return (
    <ThemeProviderContext.Provider {...props} value={value}>
      {children}
    </ThemeProviderContext.Provider>
  );
}

export const useTheme = () => {
  const context = useContext(ThemeProviderContext);

  if (context === undefined) {
    throw new Error("useTheme must be used within a ThemeProvider");
  }

  return context;
};
