/*
  Original: https://github.com/joakimbeng/split-css-selector
  MIT License © Joakim Carlstein
*/

'use strict';

export function splitSelectors(selectors) {
  if (isAtRule(selectors))
    return [selectors];

  return split(selectors, {
    splitter: ',',
    appendSplitter: false,
  });
}

export function splitSelectorParts(selector) {
  if (isAtRule(selector))
    return [selector];

  return split(selector, {
    splitters: [
      ' ', '\t', '\n', // https://developer.mozilla.org/en-US/docs/Web/CSS/Descendant_combinator
      '>', // https://developer.mozilla.org/en-US/docs/Web/CSS/Child_combinator
      '+', // https://developer.mozilla.org/en-US/docs/Web/CSS/Adjacent_sibling_combinator
      '~', // https://developer.mozilla.org/en-US/docs/Web/CSS/General_sibling_combinator
      // '||', // https://developer.mozilla.org/en-US/docs/Web/CSS/Column_combinator
    ],
    appendSplitter: true,
  }).filter(part => part !== '');
}

function split(input, { splitter, splitters, appendSplitter }) {
  const splittersSet = splitters && new Set(splitters);

  const splitted = [];
  let parens     = 0;
  let angulars   = 0;
  let soFar      = '';
  let escaped    = false;
  let singleQuoted = false;
  let doubleQuoted = false;
  for (const char of input) {
    if (escaped) {
      soFar += char;
      escaped = false;
      continue;
    }
    if (char === '\\' && !escaped) {
      soFar += char;
      escaped = true;
      continue;
    }
    if (char === "'") {
      if (singleQuoted)
        singleQuoted = false;
      else if (!doubleQuoted)
        singleQuoted = true;
    }
    else if (char === '"') {
      if (doubleQuoted)
        doubleQuoted = false;
      else if (!singleQuoted)
        doubleQuoted = true;
    }
    else if (char === '(') {
      parens++;
    }
    else if (char === ')') {
      parens--;
    }
    else if (char === '[') {
      angulars++;
    }
    else if (char === ']') {
      angulars--;
    }
    else if (splitter && char === splitter) {
      if (!parens &&
          !angulars &&
          !singleQuoted &&
          !doubleQuoted) {
        splitted.push(soFar.trim());
        soFar = '';
        if (appendSplitter)
          soFar += char;
        continue;
      }
    }
    else if (splittersSet && splittersSet.has(char)) {
      if (!parens &&
          !angulars &&
          !singleQuoted &&
          !doubleQuoted) {
        splitted.push(soFar);
        soFar = '';
        if (appendSplitter)
          soFar += char;
        continue;
      }
    }
    soFar += char;
  }
  splitted.push(soFar.trim());
  return splitted;
}

function isAtRule(selector) {
  return selector.startsWith('@');
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/Pseudo-elements
const PSEUDO_ELEMENTS = `
  :after
  ::after
  ::backdrop
  :before
  ::before
  ::cue
  ::cue-region
  :first-letter
  ::first-letter
  :first-line
  ::first-line
  ::file-selector-button
  ::grammer-error
  ::marker
  ::part
  ::placeholder
  ::selection
  ::slotted
  ::spelling-error
  ::target-text
`.trim().split('\n').map(item => item.trim());
const PSEUDO_ELEMENTS_MATCHER = new RegExp(`(${PSEUDO_ELEMENTS.join('|')})$`, 'i');

export function appendPart(baseSelector, appendant) {
  if (PSEUDO_ELEMENTS_MATCHER.test(baseSelector))
    return baseSelector.replace(PSEUDO_ELEMENTS_MATCHER, `${appendant}$1`);
  return `${baseSelector}${appendant}`;
}
