# Onboarding

System addon to provide the onboarding overlay for user friendly tours.

## Architecture

Everytime `about:home` or `about:newtab` page is opened, onboarding overlay is injected into that page (if `browser.onboarding.enabled` preference is `true`).

## Landing rules

We would apply some rules:

* Avoid `chrome://` in `onbaording.js` since onboarding is intented to be injected into a normal content process page.
* All styles and ids should be formated as `onboarding-*` to avoid conflict with the origin page.
* All strings in `locales` should be formated as `onboarding.*` for consistency.

## References

Content process related change:
* The `overlay-close.svg` comes from `browser/themes/shared/sidebar/close.svg`.
