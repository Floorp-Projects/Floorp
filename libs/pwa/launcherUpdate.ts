/** Refresh an OS launcher while changing the associated PWA store key. */
export async function refreshLauncherForStoreMove<T>(
  launcher: { install(manifest: T, strictIcon?: boolean): Promise<void> },
  store: { moveSsbKey(oldKey: string, manifest: T): Promise<boolean> },
  oldKey: string,
  oldManifest: T,
  updatedManifest: T,
): Promise<boolean> {
  try {
    // First-time installation may fall back to the browser icon. A refresh
    // must fail if it cannot update the existing icon/container badge.
    await launcher.install(updatedManifest, true);
    if (await store.moveSsbKey(oldKey, updatedManifest)) {
      return true;
    }
  } catch (error) {
    try {
      await launcher.install(oldManifest, true);
    } catch (restoreError) {
      throw new AggregateError(
        [error, restoreError],
        "Failed to restore the original PWA launcher",
      );
    }
    throw error;
  }

  await launcher.install(oldManifest, true);
  return false;
}
