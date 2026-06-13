import { assertEquals } from "jsr:@std/assert";
import {
  getContainerColorHex,
  mapColorToCSSVariable,
  resolveContainerDisplayColor,
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
