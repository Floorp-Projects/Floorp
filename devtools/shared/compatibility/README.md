# Compatibility Dataset

## How to update the MDN compatibility data

The Compatibility panel detects issues by comparing against official [MDN compatibility data](https://github.com/mdn/browser-compat-data). It uses a local snapshot of the dataset. This dataset needs to be manually synchronized periodically to `devtools/shared/compatibility/dataset` (ideally with every Firefox release).

The subsets from the dataset required by the Compatibility panel are:

- browsers: [https://github.com/mdn/browser-compat-data/tree/master/browsers](https://github.com/mdn/browser-compat-data/tree/master/browsers)
- css.properties: [https://github.com/mdn/browser-compat-data/tree/master/css](https://github.com/mdn/browser-compat-data/tree/master/css).

In order to download up-to-date data, you need to run the following commands:

- `cd devtools/shared/compatibility`
- `yarn install` and select the latest package version
- `yarn update`

This should save the JSON files directly in `devtools/shared/compatibility/dataset/`.

Check that all tests still pass. It is possible that changes in the structure or contents of the latest dataset will cause tests to fail. If that is the case, fix the tests. **Do not manually change the contents or structure of the local dataset** because any changes will be overwritten by the next update from the official dataset.
