# UPGRADING

1. `git clone https://github.com/beautify-web/js-beautify.git`

2. Copy `js/lib/beautify.js` to `devtools/shared/jsbeautify/src/beautify-js.js`

3. Remove the acorn section from the file and add the following to the top:

 ```
 const acorn = require("acorn/acorn");
 ```

4. Just above `function Beautifier(js_source_text, options) {` add:

 ```
 exports.jsBeautify = js_beautify;
 ```

5. Copy `beautify-html.js` to `devtools/shared/jsbeautify/src/beautify-html.js`

6. Replace the require blocks at the bottom of the file with:

 ```
 var beautify = require('devtools/shared/jsbeautify/beautify');

 exports.htmlBeautify = function(html_source, options) {
    return style_html(html_source, options, beautify.js, beautify.css);
 };
 ```

7. Copy `beautify-css.js` to `devtools/shared/jsbeautify/src/beautify-css.js`

8. Replace the global define block at the bottom of the file with:
 ```
 exports.cssBeautify = css_beautify;
 ```
9. Copy `js/test/beautify-tests.js` to `devtools/shared/jsbeautify/src/beautify-tests.js`
