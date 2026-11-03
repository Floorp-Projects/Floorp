// SPDX-License-Identifier: MPL-2.0
export interface AuxiliaryFontFace {
  family: string;
  name: string;
}
export interface AuxiliaryScriptFont {
  fontId: string;
  latin: AuxiliaryFontFace[];
  cyrillic: AuxiliaryFontFace[];
}
export interface AuxiliaryFontPair {
  fontIds: [string, string];
  os: string;
  installedFontCount: number;
  baselineFaces: AuxiliaryFontFace[];
  latinOverride: AuxiliaryScriptFont;
  cyrillicOverride: AuxiliaryScriptFont;
}
export interface AuxiliaryFontWindow extends Window {
  InspectorUtils: {
    getUsedFontFaces(range: Range): Array<{
      CSSFamilyName: string;
      name: string;
    }>;
  };
}
