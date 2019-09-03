"use strict";

// This tests searching in the urlbar (a.k.a. the quantumbar).

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */

/* These reflows happen only the first time the panel opens. */
const EXPECTED_REFLOWS_FIRST_OPEN = [];

/* These reflows happen every time the panel opens. */
const EXPECTED_REFLOWS_SECOND_OPEN = [];

add_task(async function quantumbar() {
  await runUrlbarTest(
    true,
    EXPECTED_REFLOWS_FIRST_OPEN,
    EXPECTED_REFLOWS_SECOND_OPEN
  );
});
