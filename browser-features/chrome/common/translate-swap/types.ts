// SPDX-License-Identifier: MPL-2.0

export type LanguageSelection = {
  from: string;
  to: string;
  fromOptions: readonly string[];
  toOptions: readonly string[];
};

export type NativeMenuList = Element & { value: string };

/** The public surface of Firefox's SelectTranslationsPanel that we rely on. */
export type SelectTranslationsPanelApi = {
  onChangeFromLanguage: () => void;
  onChangeToLanguage: () => void;
  phase: () => string;
};

export type TranslateSwapDocument = Document & {
  createXULElement: (tagName: string) => Element;
};

export type TranslateSwapControllerOptions = {
  document: TranslateSwapDocument;
  /** Resolved lazily: Firefox loads the panel script on first access. */
  getPanelApi: () => SelectTranslationsPanelApi | null;
  label: string;
};
