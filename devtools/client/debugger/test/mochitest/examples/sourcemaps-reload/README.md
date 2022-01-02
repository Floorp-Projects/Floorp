### Steps to Rebuild

1. make changes to v1.js, v2.js, v3.js
2. run `yarn` to install webpack & babel
3. run `webpack`
4. change `sources` reference in `v2.bundle.js.map` to `v1.js`
5. change `sources` reference in `v3.bundle.js.map` to `v1.js`
