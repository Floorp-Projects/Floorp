export function enableASRouterContent(store, asrouterContent) {
  // Enable asrouter content
  store.subscribe(() => {
    const state = store.getState();
    if (!state.ASRouter.initialized) {
      return;
    }

    if (!asrouterContent.initialized) {
      asrouterContent.init(store);
    }
  });
  // Return this for testing purposes
  return {asrouterContent};
}
