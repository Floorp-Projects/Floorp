import {
  ChakraProvider,
  createSystem,
  defaultBaseConfig,
  defineConfig,
} from "@chakra-ui/react";
import type { PropsWithChildren } from "react";

const system = createSystem(
  defaultBaseConfig,
  defineConfig({
    // Tailwind still resets the legacy pages during the incremental migration.
    preflight: false,
    globalCss: {},
    theme: {
      tokens: {
        fonts: {
          body: { value: "var(--floorp-font)" },
          heading: { value: "var(--floorp-font)" },
        },
        colors: {
          floorp: {
            canvas: { value: "var(--floorp-canvas)" },
            surface: { value: "var(--floorp-surface)" },
            ink: { value: "var(--floorp-ink)" },
            line: { value: "var(--floorp-line)" },
            brand: { value: "var(--floorp-brand)" },
          },
        },
      },
    },
  }),
);

export function FloorpUIProvider({ children }: PropsWithChildren) {
  return <ChakraProvider value={system}>{children}</ChakraProvider>;
}
