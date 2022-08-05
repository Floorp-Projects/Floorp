/**
 * By default this test only tests 1 sample. This is to avoid publishing all samples we have
 * to the codebase. If you update the Fathom CC model, please follow the instruction below
 * and run the test. Doing this makes sure the optimized (Native implementation) CC fathom model produces
 * exactly the same result as the non-optimized model (JS implementation, See CreditCardRuleset.jsm).
 *
 * To test this:
 * 1. Run the test setup script (fathom/test-setup.sh) to download all samples to the local
 *    directory. Note that you need to have the permission to access the fathom-form-autofill
 * 2. Set `gTestAutofillRepoSample` to true
 * 3. Run this test
 */

"use strict";

const eligibleElementSelector =
  "input:not([type]), input[type=text], input[type=textbox], input[type=email], input[type=tel], input[type=number], input[type=month], select, button";

const skippedSamples = [
  // TOOD: Crash while running the following testcases. Since this is not caused by the fathom CC
  // model, we just skip those for now
  "EN_B105b.html",
  "EN_B312a.html",
  "EN_B118b.html",
  "EN_B48c.html",

  // This sample is skipped because of Bug 1754256 (Support lookaround regex for native fathom CC implementation).
  "DE_B378b.html",
];

async function run_test(path, dirs) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.formautofill.creditCards.heuristics.mode", 1]],
  });

  // Collect files we are going to test.
  let files = [];
  for (let dir of dirs) {
    let entries = new FileUtils.File(getTestFilePath(path + dir))
      .directoryEntries;

    while (entries.hasMoreElements()) {
      let entry = entries.nextFile;
      if (skippedSamples.includes(entry.leafName)) {
        continue;
      }

      if (entry.leafName.endsWith(".html")) {
        files.push(path + dir + entry.leafName);
      }
    }
  }

  ok(files.length, "no sample files found");

  let summary = {};
  for (let file of files) {
    info("Testing " + file + "...");

    await BrowserTestUtils.withNewTab(BASE_URL + file, async browser => {
      summary[file] = await SpecialPowers.spawn(
        browser,
        [{ eligibleElementSelector, file }],
        obj => {
          const { FieldScanner } = ChromeUtils.import(
            "resource://autofill/FormAutofillHeuristics.jsm"
          );
          const { FormAutofillUtils } = ChromeUtils.import(
            "resource://autofill/FormAutofillUtils.jsm"
          );

          let eligibleFields = [];
          let nodeList = content.document.querySelectorAll(
            obj.eligibleElementSelector
          );
          for (let i = 0; i < nodeList.length; i++) {
            if (FormAutofillUtils.isCreditCardOrAddressFieldType(nodeList[i])) {
              eligibleFields.push(nodeList[i]);
            }
          }
          let failedFields = [];

          info("Running CC fathom model");
          let nativeConfidencesKeyedByType = ChromeUtils.getFormAutofillConfidences(
            eligibleFields
          );
          let jsConfidencesKeyedByType = FieldScanner.getFormAutofillConfidences(
            eligibleFields
          );

          if (eligibleFields.length != nativeConfidencesKeyedByType.length) {
            ok(
              false,
              `Get the wrong number of confidence value from the native model`
            );
          }
          if (eligibleFields.length != jsConfidencesKeyedByType.length) {
            ok(
              false,
              `Get the wrong number of confidence value from the js model`
            );
          }

          // This value should sync with the number of supported types in
          // CreditCardRuleset.jsm (See `get types()` in `this.creditCardRulesets`).
          const EXPECTED_NUM_OF_CONFIDENCE = 1;
          for (let i = 0; i < eligibleFields.length; i++) {
            if (
              Object.keys(nativeConfidencesKeyedByType[i]).length !=
              EXPECTED_NUM_OF_CONFIDENCE
            ) {
              ok(
                false,
                `Native CC model doesn't get confidence value for all types`
              );
            }
            if (
              Object.keys(jsConfidencesKeyedByType[i]).length !=
              EXPECTED_NUM_OF_CONFIDENCE
            ) {
              ok(
                false,
                `JS CC model doesn't get confidence value for all types`
              );
            }

            for (let [type, confidence] of Object.entries(
              nativeConfidencesKeyedByType[i]
            )) {
              // Fix to 10 digit to ignore rounding error between js and c++.
              let nativeConfidence = confidence.toFixed(10);
              let jsConfidence = jsConfidencesKeyedByType[i][
                FormAutofillUtils.formAutofillConfidencesKeyToCCFieldType(type)
              ].toFixed(10);
              if (jsConfidence != nativeConfidence) {
                info(
                  `${obj.file}: Element(id=${eligibleFields[i].id} doesn't have the same confidence value when rule type is ${type}`
                );
                if (!failedFields.includes(i)) {
                  failedFields.push(i);
                }
              }
            }
          }
          ok(
            !failedFields.length,
            `${obj.file}: has the same confidences value on both models`
          );
          return {
            tested: eligibleFields.length,
            passed: eligibleFields.length - failedFields.length,
          };
        }
      );
    });
  }

  // Generating summary report
  let total_tested_samples = 0;
  let total_passed_samples = 0;
  let total_tested_fields = 0;
  let total_passed_fields = 0;
  info("=====Summary=====");
  for (const [k, v] of Object.entries(summary)) {
    total_tested_samples++;
    if (v.tested == v.passed) {
      total_passed_samples++;
    } else {
      info("Failed Case:" + k);
    }
    total_tested_fields += v.tested;
    total_passed_fields += v.passed;
  }
  info(
    "Passed Samples/Test Samples: " +
      total_passed_samples +
      "/" +
      total_tested_samples
  );
  info(
    "Passed Fields/Test Fields: " +
      total_passed_fields +
      "/" +
      total_tested_fields
  );
}

add_task(async function test_native_cc_model() {
  const path = "fathom/";
  const dirs = ["testing/"];
  await run_test(path, dirs);
});

add_task(async function test_native_cc_model_autofill_repo() {
  const path = "fathom/autofill-repo-samples/";
  const dirs = ["validation/", "training/", "testing/"];
  if (await IOUtils.exists(getTestFilePath(path))) {
    // Just to ignore timeout failure while running the test on the local
    requestLongerTimeout(10);

    await run_test(path, dirs);
  }
});
