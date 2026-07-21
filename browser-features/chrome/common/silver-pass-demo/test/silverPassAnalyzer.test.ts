// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  analyzeSilverPassText,
  getKnownSilverPassTermIds,
} from "../silver-pass-analyzer.ts";

const COMPLETE_DEMO_TEXT = [
  "高齢者移動支援券",
  "住民税非課税世帯",
  "交付申請",
  "本人確認書類",
  "利用者証明用電子証明書",
  "所得・課税証明書",
].join(" ");

const EXPECTED_TERM_IDS = [
  "resident-tax-exempt-household",
  "issuance-application",
  "identity-verification-document",
  "user-authentication-certificate",
  "income-taxation-certificate",
] as const;

const tests: TestCase[] = [
  {
    name: "matches all five known administrative terms in rule order",
    fn() {
      const analysis = analyzeSilverPassText(COMPLETE_DEMO_TEXT);

      assert(analysis !== null, "complete demo text should be supported");
      assertEquals(
        JSON.stringify(analysis.terms.map((term) => term.id)),
        JSON.stringify(EXPECTED_TERM_IDS),
        "all five term ids should be returned in deterministic rule order",
      );
      assertEquals(
        JSON.stringify(getKnownSilverPassTermIds()),
        JSON.stringify(EXPECTED_TERM_IDS),
        "the known-term helper should expose the same ordered ids",
      );
    },
  },
  {
    name: "returns the deterministic purpose and next target",
    fn() {
      const analysis = analyzeSilverPassText(COMPLETE_DEMO_TEXT);

      assert(analysis !== null, "complete demo text should be supported");
      assertEquals(
        analysis.purposeKey,
        "silverPassDemo.demoPurpose",
        "purpose key should be fixed for the demo",
      );
      assertEquals(
        analysis.nextStepKey,
        "silverPassDemo.demoNextStep",
        "next-step key should be fixed for the demo",
      );
      assertEquals(
        analysis.reasonKey,
        "silverPassDemo.demoReason",
        "reason key should be fixed for the demo",
      );
      assertEquals(
        analysis.targetKey,
        "apply-start",
        "the demo should point at the non-mutating start target",
      );
    },
  },
  {
    name: "rejects text without both required page signals",
    fn() {
      const missingServiceName = COMPLETE_DEMO_TEXT.replace(
        "高齢者移動支援券",
        "別の手続き",
      );
      const missingApplicationSignal = COMPLETE_DEMO_TEXT.replace(
        "交付申請",
        "手続き",
      );

      assertEquals(
        analyzeSilverPassText(missingServiceName),
        null,
        "the municipal service name is required",
      );
      assertEquals(
        analyzeSilverPassText(missingApplicationSignal),
        null,
        "the application signal is required",
      );
    },
  },
  {
    name: "rejects pages with fewer than three known terms",
    fn() {
      const tooFewTerms = [
        "高齢者移動支援券",
        "交付申請",
        "本人確認書類",
      ].join(" ");

      assertEquals(
        analyzeSilverPassText(tooFewTerms),
        null,
        "two matched terms should not be enough to identify the demo",
      );
    },
  },
  {
    name: "normalizes compatibility characters before matching",
    fn() {
      const compatibilityText = COMPLETE_DEMO_TEXT.replace(
        "利用者証明用電子証明書",
        "利用者証明用電子証明書",
      );
      const analysis = analyzeSilverPassText(compatibilityText);

      assert(
        analysis !== null,
        "a compatibility ideograph should match after NFKC normalization",
      );
      assertEquals(
        analysis.terms.length,
        EXPECTED_TERM_IDS.length,
        "normalization should retain every matched term",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("silverPassAnalyzer.test.ts", tests);
}
