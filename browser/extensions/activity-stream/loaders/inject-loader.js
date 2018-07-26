// Note: this is based on https://github.com/plasticine/inject-loader,
// patched to make istanbul work properly

const loaderUtils = require("loader-utils");
const QUOTE_REGEX_STRING = "['|\"]{1}";

const hasOnlyExcludeFlags = query => Object.keys(query).filter(key => query[key] === true).length === 0;
const escapePath = path => path.replace("/", "\\/");

function createRequireStringRegex(query) {
  const regexArray = [];

  // if there is no query then replace everything
  if (Object.keys(query).length === 0) {
    regexArray.push("([^\\)]+)");
  } else if (hasOnlyExcludeFlags(query)) {
  // if there are only negation matches in the query then replace everything
  // except them
    Object.keys(query).forEach(key => regexArray.push(`(?!${QUOTE_REGEX_STRING}${escapePath(key)})`));
    regexArray.push("([^\\)]+)");
  } else {
    regexArray.push(`(${QUOTE_REGEX_STRING}(`);
    regexArray.push(Object.keys(query).map(key => escapePath(key)).join("|"));
    regexArray.push(`)${QUOTE_REGEX_STRING})`);
  }

  // Wrap the regex to match `require()`
  regexArray.unshift("require\\(");
  regexArray.push("\\)");

  return new RegExp(regexArray.join(""), "g");
}

module.exports = function inject(src) {
  if (this.cacheable) {
    this.cacheable();
  }
  const regex = createRequireStringRegex(loaderUtils.parseQuery(this.query));

  return `module.exports = function inject(injections) {
  var module = {exports: {}};
  var exports = module.exports;
  ${src.replace(regex, "(injections[$1] || /* istanbul ignore next */ $&)")}
  return module.exports;
}\n`;
};
