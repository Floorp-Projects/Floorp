// SPDX-License-Identifier: MPL-2.0
export async function withMenuCleanup(
  check: () => Promise<void>,
  cleanup: () => Promise<void>,
): Promise<void> {
  let failed = false;
  let firstError: unknown;
  try {
    await check();
  } catch (error) {
    failed = true;
    firstError = error;
  }
  try {
    await cleanup();
  } catch (error) {
    console.error(
      `[nora@workspace-menu-cleanup] ${
        encodeURIComponent(JSON.stringify({
          error: error instanceof Error ? error.message : String(error),
        }))
      }`,
    );
    if (!failed) {
      failed = true;
      firstError = error;
    }
  }
  if (failed) throw firstError;
}
