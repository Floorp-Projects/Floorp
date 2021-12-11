# UPGRADING

1. `git clone https://github.com/beautify-web/js-beautify.git`

2. `cd js-beautify`

3. Retrieve the latest tag with

```
git describe --tags `git rev-list --tags --max-count=1`
```

4. Move to the latest tag `git checkout ${latestTag}` (`${latestTag}` should be replaced with
   what was printed at step 3).

5. `npm install`

6. `npx webpack`

7. Copy `js/lib/beautify.js` to `devtools/shared/jsbeautify/src/beautify-js.js`

8. Copy `js/lib/beautify-html.js` to `devtools/shared/jsbeautify/src/beautify-html.js`

9. Replace the following line at the bottom of the file:

```
var js_beautify = require('./beautify.js');
```

with (changing `beautify.js` into `beautify-js.js`):

```
var js_beautify = require('./beautify-js.js');
```

10. Copy `js/lib/beautify-css.js` to `devtools/shared/jsbeautify/src/beautify-css.js`
