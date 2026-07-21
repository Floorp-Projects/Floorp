// SPDX-License-Identifier: MPL-2.0

export type SilverPassStatus =
  | "idle"
  | "loading"
  | "ready"
  | "unsupported"
  | "error";

export type SilverPassTargetKey = "apply-start";

export type SilverPassTermId =
  | "resident-tax-exempt-household"
  | "issuance-application"
  | "identity-verification-document"
  | "user-authentication-certificate"
  | "income-taxation-certificate";

export type SilverPassI18nKey = `silverPassDemo.${string}`;

export interface SilverPassTerm {
  id: SilverPassTermId;
  labelKey: SilverPassI18nKey;
  explanationKey: SilverPassI18nKey;
}

export interface SilverPassAnalysis {
  purposeKey: SilverPassI18nKey;
  nextStepKey: SilverPassI18nKey;
  reasonKey: SilverPassI18nKey;
  targetKey: SilverPassTargetKey;
  terms: SilverPassTerm[];
}

export interface SilverPassViewState {
  status: SilverPassStatus;
  analysis: SilverPassAnalysis | null;
  messageKey: SilverPassI18nKey | null;
}

export interface SilverPassPageClient {
  waitForReady(): Promise<boolean>;
  isVisible(selector: string): Promise<boolean>;
  getScopedText(selector: string): Promise<string | null>;
  clearEffects(): Promise<boolean>;
  scrollToElement(selector: string): Promise<boolean>;
}

export interface SilverPassPageContext {
  url: string;
  createClient(): SilverPassPageClient | null;
  isCurrent?(): boolean;
}

export type SilverPassPageContextProvider = () => SilverPassPageContext;
