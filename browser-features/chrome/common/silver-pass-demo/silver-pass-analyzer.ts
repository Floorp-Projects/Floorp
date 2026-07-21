// SPDX-License-Identifier: MPL-2.0

import type {
  SilverPassAnalysis,
  SilverPassTerm,
  SilverPassTermId,
} from "./types.ts";

type TermRule = SilverPassTerm & {
  needle: string;
};

const TERM_RULES = [
  {
    id: "resident-tax-exempt-household",
    needle: "住民税非課税世帯",
    labelKey: "silverPassDemo.termResidentTaxExemptHousehold",
    explanationKey: "silverPassDemo.termResidentTaxExemptHouseholdExplanation",
  },
  {
    id: "issuance-application",
    needle: "交付申請",
    labelKey: "silverPassDemo.termIssuanceApplication",
    explanationKey: "silverPassDemo.termIssuanceApplicationExplanation",
  },
  {
    id: "identity-verification-document",
    needle: "本人確認書類",
    labelKey: "silverPassDemo.termIdentityVerificationDocument",
    explanationKey:
      "silverPassDemo.termIdentityVerificationDocumentExplanation",
  },
  {
    id: "user-authentication-certificate",
    needle: "利用者証明用電子証明書",
    labelKey: "silverPassDemo.termUserAuthenticationCertificate",
    explanationKey:
      "silverPassDemo.termUserAuthenticationCertificateExplanation",
  },
  {
    id: "income-taxation-certificate",
    needle: "所得・課税証明書",
    labelKey: "silverPassDemo.termIncomeTaxationCertificate",
    explanationKey: "silverPassDemo.termIncomeTaxationCertificateExplanation",
  },
] as const satisfies readonly TermRule[];

const REQUIRED_PAGE_SIGNALS = ["高齢者移動支援券", "交付申請"] as const;
const MINIMUM_TERM_MATCHES = 3;

export function analyzeSilverPassText(
  text: string,
): SilverPassAnalysis | null {
  const normalizedText = text.normalize("NFKC");
  if (
    !REQUIRED_PAGE_SIGNALS.every((signal) => normalizedText.includes(signal))
  ) {
    return null;
  }

  const terms = TERM_RULES.filter((rule) =>
    normalizedText.includes(rule.needle)
  ).map(({ needle: _needle, ...term }) => term);

  if (terms.length < MINIMUM_TERM_MATCHES) {
    return null;
  }

  return {
    purposeKey: "silverPassDemo.demoPurpose",
    nextStepKey: "silverPassDemo.demoNextStep",
    reasonKey: "silverPassDemo.demoReason",
    targetKey: "apply-start",
    terms,
  };
}

export function getKnownSilverPassTermIds(): readonly SilverPassTermId[] {
  return TERM_RULES.map((rule) => rule.id);
}
