# UPGRADING

1. `git clone https://github.com/beautify-web/js-beautify.git`

2. Copy `js/lib/beautify.js` to `devtools/shared/jsbeautify/src/beautify-js.js`

3. Copy `beautify-html.js` to `devtools/shared/jsbeautify/src/beautify-html.js`

4. Replace the following line at the bottom of the file:

```
var js_beautify = require('./beautify.js');
```

with (changing `beautify.js` into `beautify-js.js`):

```
var js_beautify = require('./beautify-js.js');
```

6. Copy `beautify-css.js` to `devtools/shared/jsbeautify/src/beautify-css.js`
