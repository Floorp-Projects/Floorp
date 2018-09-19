export function enableASRouterContent(store, asrouterContent) {
  // Enable asrouter content
  store.subscribe(() => {
    const state = store.getState();
    if (state.Prefs.values.asrouterExperimentEnabled && !asrouterContent.initialized) {
      asrouterContent.init();
    } else if (!state.Prefs.values.asrouterExperimentEnabled && asrouterContent.initialized) {
      asrouterContent.uninit();
    }
  });
  // Return this for testing purposes
  return {asrouterContent};
}
