# Compatibility Panel

## Related files
The compatibility panel consists of the following files:
* Client:
  * Main: `devtools/client/inspector/compatibility/`
  * Style: `devtools/client/themes/compatibility.css`
* Shared:
  * MDN compatibility dataset: `devtools/shared/compatibility/dataset/`
  * MDN compatibility library: `devtools/server/actors/compatibility/lib/MDNCompatibility.js`
  * User setting file - `devtools/shared/compatibility/compatibility-user-settings.js`
* Server:
  * Actor: `devtools/server/actors/compatibility.js`
  * Front: `devtools/client/fronts/compatibility.js`
  * Spec: `devtools/shared/specs/compatibility.js`

## MDN Compatibility Data
The Compatibility panel detects issues by comparing against official [MDN compatibility data](https://github.com/mdn/browser-compat-data). It uses a local snapshot of the dataset. This dataset needs to be manually synchronized periodically to `devtools/shared/compatibility/dataset` (ideally with every Firefox release).

To update this dataset, please refer to the guidelines in `devtools/shared/compatibility/README.md`

## Tests
* Client: `devtools/client/inspector/compatibility/test`
* MDN compatibility library: `devtools/server/actors/compatibility/lib/test`
* Server: `devtools/server/tests/browser/browser_compatibility_cssIssues.js`
