// SPDX-License-Identifier: MPL-2.0
import type {
  AuxiliaryFontFace,
  AuxiliaryFontPair,
  AuxiliaryFontWindow,
  AuxiliaryScriptFont,
} from "../helpers/auxiliary-font-test-types.ts";

// Select inputs using real installed glyph faces before exercising the public
// extension API. Font enumeration order does not imply script coverage.
export async function selectAuxiliaryFontPair(): Promise<AuxiliaryFontPair> {
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as AuxiliaryFontWindow | null;
  if (!win) throw new Error("Actual font selection requires a browser window");
  const fonts = await Cc["@mozilla.org/gfx/fontenumerator;1"].getService(
    Ci.nsIFontEnumerator,
  ).EnumerateAllFontsAsync();
  const available = new Set(fonts);
  const candidates = Services.appinfo.OS === "Linux"
    ? [["DejaVu Sans", "DejaVu Serif"], ["Arial", "Times New Roman"]]
    : [["Arial", "Times New Roman"], ["DejaVu Sans", "DejaVu Serif"]];
  const stage = win.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "span",
  );
  stage.style.cssText =
    "position:fixed;top:0;left:0;font-size:40px;opacity:0.01";
  win.document.documentElement.appendChild(stage);
  const faces = (text: string, family: string): AuxiliaryFontFace[] => {
    stage.textContent = text;
    stage.style.fontFamily = family;
    stage.getBoundingClientRect();
    const range = win.document.createRange();
    range.selectNodeContents(stage);
    return win.InspectorUtils.getUsedFontFaces(range).map((face) => ({
      family: face.CSSFamilyName,
      name: face.name,
    }));
  };
  const inspect = (fontId: string): AuxiliaryScriptFont => ({
    fontId,
    latin: faces("Latin sample", JSON.stringify(fontId)),
    cyrillic: faces("Кириллица", JSON.stringify(fontId)),
  });
  const rendered = (font: AuxiliaryScriptFont) =>
    [font.latin, font.cyrillic].every((items) =>
      items.length > 0 &&
      items.every((face) =>
        face.family.toLowerCase() === font.fontId.toLowerCase() && face.name
      )
    );
  const fingerprint = (items: AuxiliaryFontFace[]) =>
    [...new Set(items.map((face) => face.name))].sort().join("\n");
  try {
    const baselineFaces = faces("Latin sample", "serif");
    const selected = candidates.filter((pair) =>
      pair.every((font) => available.has(font))
    ).map(([a, b]) => ({ a: inspect(a), b: inspect(b) })).find((pair) =>
      rendered(pair.a) && rendered(pair.b) &&
      fingerprint(pair.a.latin) !== fingerprint(pair.b.latin) &&
      fingerprint(pair.a.cyrillic) !== fingerprint(pair.b.cyrillic) &&
      fingerprint(pair.a.latin) !== fingerprint(baselineFaces)
    );
    if (!selected) {
      throw new Error(
        "Actual installed Latin/Cyrillic fonts must provide distinct glyph faces: " +
          JSON.stringify({
            os: Services.appinfo.OS,
            candidates,
            baselineFaces,
            availablePreferred: candidates.flat().filter((font) =>
              available.has(font)
            ),
          }),
      );
    }
    return {
      fontIds: [selected.a.fontId, selected.b.fontId],
      os: Services.appinfo.OS,
      installedFontCount: fonts.length,
      baselineFaces,
      latinOverride: selected.a,
      cyrillicOverride: selected.b,
    };
  } finally {
    stage.remove();
  }
}
