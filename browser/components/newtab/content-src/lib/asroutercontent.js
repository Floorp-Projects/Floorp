export function enableASRouterContent(store, asrouterContent) {
  // Enable asrouter content
  store.subscribe(() => {
    const state = store.getState();
    if (!state.ASRouter.initialized) {
      return;
    }

    if (!asrouterContent.initialized) {
      asrouterContent.init();
    }
  });
  // Return this for testing purposes
  return {asrouterContent};
}
