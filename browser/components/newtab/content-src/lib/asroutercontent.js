export function enableASRouterContent(store, asrouterContent) {
  let didHideOnboarding = false;

  // Enable asrouter content
  store.subscribe(() => {
    const state = store.getState();
    if (!state.ASRouter.initialized) {
      return;
    }

    if (!asrouterContent.initialized) {
      asrouterContent.init();
    }

    // Until we can delete the existing onboarding tour, just hide the onboarding button when users are in
    // the new simplified onboarding experiment. CSS hacks ftw
    if (state.ASRouter.allowLegacyOnboarding === false && !didHideOnboarding) {
      global.document.body.classList.add("hide-onboarding");
      didHideOnboarding = true;
    } else if (state.ASRouter.allowLegacyOnboarding === true && didHideOnboarding) {
      global.document.body.classList.remove("hide-onboarding");
      didHideOnboarding = false;
    }
  });
  // Return this for testing purposes
  return {asrouterContent};
}
