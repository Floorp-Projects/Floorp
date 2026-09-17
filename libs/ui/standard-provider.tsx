import {
  ChakraProvider,
  createSystem,
  defaultConfig,
  defineConfig,
} from "@chakra-ui/react";
import type { PropsWithChildren } from "react";
import { StandardControlsContext } from "./control-theme.ts";

// Keep Chakra's recipes and semantic tokens without resetting adjacent legacy UI.
const system = createSystem(
  { ...defaultConfig, preflight: false, globalCss: {} },
  defineConfig({
    theme: {
      tokens: {
        fonts: {
          body: { value: "var(--floorp-font)" },
          heading: { value: "var(--floorp-font)" },
        },
      },
    },
  }),
);

export function StandardUIProvider({ children }: PropsWithChildren) {
  return (
    <ChakraProvider value={system}>
      <StandardControlsContext.Provider value={true}>
        {children}
      </StandardControlsContext.Provider>
    </ChakraProvider>
  );
}
