import { assertEquals } from "jsr:@std/assert";
import {
  getContainerColorHex,
  mapColorToCSSVariable,
  resolveContainerDisplayColor,
  resolveContainerDisplayColorFromWindow,
} from "./containerColorMap.ts";

Deno.test("mapColorToCSSVariable maps numeric colors", () => {
  assertEquals(mapColorToCSSVariable(0), "blue");
  assertEquals(mapColorToCSSVariable(1), "turquoise");
  assertEquals(mapColorToCSSVariable(5), "red");
});

Deno.test("mapColorToCSSVariable maps string color names", () => {
  assertEquals(mapColorToCSSVariable("green"), "green");
  assertEquals(mapColorToCSSVariable("BLUE"), "blue");
});

Deno.test("mapColorToCSSVariable returns null for unknown values", () => {
  assertEquals(mapColorToCSSVariable(null), null);
  assertEquals(mapColorToCSSVariable(99), null);
});

Deno.test("getContainerColorHex returns fallback for known colors", () => {
  assertEquals(getContainerColorHex("blue"), "#37adff");
  assertEquals(getContainerColorHex("unknown"), "#37adff");
});

Deno.test("resolveContainerDisplayColor falls back to hex without document", () => {
  assertEquals(resolveContainerDisplayColor("red", null), "#ff613d");
  assertEquals(resolveContainerDisplayColor(null, null), "#37adff");
});

Deno.test("resolveContainerDisplayColorFromWindow reads usercontext.css vars", () => {
  const mockWin = {
    document: {
      documentElement: {
        appendChild: () => {},
      },
      createElement: () => ({
        className: "",
        hidden: false,
        remove: () => {},
      }),
    },
    getComputedStyle: (element: unknown) => ({
      getPropertyValue: (prop: string) => {
        if (
          (element as { className?: string }).className === "identity-color-red" &&
          prop === "--identity-tab-color"
        ) {
          return "#ff613d";
        }
        return "";
      },
    }),
  };

  assertEquals(
    resolveContainerDisplayColorFromWindow("red", mockWin),
    "#ff613d",
  );
});
