### Steps to Rebuild

1. make changes to files within v1, v2, v3,... folders (update package.json if you add more folders/versions)
2. run `yarn` to install webpack & babel
3. run `yarn run pack`
4. also go rebuild bundles in ../sourcemaps-reload-compressed/

There are two distinct source folders as their package.json file is different.
They use different webpack/babel versions.
This also allows to detect the webpack version from webpack.config.js
and use different settings in order to compress or not.
