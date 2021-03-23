/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Fathom ML model for identifying the fields of credit-card forms
 *
 * This is developed out-of-tree at https://github.com/mozilla-services/fathom-
 * form-autofill, where there is also over a GB of training, validation, and
 * testing data. To make changes, do your edits there (whether adding new
 * training pages, adding new rules, or both), retrain and evaluate as
 * documented at https://mozilla.github.io/fathom/training.html, paste the
 * coefficients emitted by the trainer into the ruleset, and finally copy the
 * ruleset's "CODE TO COPY INTO PRODUCTION" section to this file's "CODE FROM
 * TRAINING REPOSITORY" section.
 */

"use strict";

/**
 * CODE UNIQUE TO PRODUCTION--NOT IN THE TRAINING REPOSITORY:
 */

const EXPORTED_SYMBOLS = ["creditCardRuleset"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "fathom",
  "resource://gre/modules/third_party/fathom/fathom.jsm"
);
const {
  element: clickedElement,
  exceptions: { NoWindowError },
  out,
  rule,
  ruleset,
  score,
  type,
  utils: { isVisible },
} = fathom;

ChromeUtils.defineModuleGetter(
  this,
  "FormLikeFactory",
  "resource://gre/modules/FormLikeFactory.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://formautofill/FormAutofillUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  SUPPORTED_NETWORKS: "resource://gre/modules/CreditCard.jsm",
  NETWORK_NAMES: "resource://gre/modules/CreditCard.jsm",
  LabelUtils: "resource://formautofill/FormAutofillUtils.jsm",
});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyGetter(this, "HeuristicsRegExp", () => {
  const sandbox = {};
  const HEURISTICS_REGEXP = "chrome://formautofill/content/heuristicsRegexp.js";
  Services.scriptloader.loadSubScript(HEURISTICS_REGEXP, sandbox);
  return sandbox.HeuristicsRegExp;
});

/**
 * Callthrough abstraction to allow .getAutocompleteInfo() to be mocked out
 * during training
 *
 * @param {Element} element DOM element to get info about
 * @returns {object} Page-author-provided autocomplete metadata
 */
function getAutocompleteInfo(element) {
  return element.getAutocompleteInfo();
}

/**
 * @param {string} selector A CSS selector that prunes away ineligible elements
 * @returns {Lhs} An LHS yielding the element the user has clicked or, if
 *  pruned, none
 */
function queriedOrClickedElements(selector) {
  return clickedElement(selector);
}

/**
 * START OF CODE PASTED FROM TRAINING REPOSITORY
 */

const MMRegExp = /^mm$|\(mm\)/i;
const YYorYYYYRegExp = /^(yy|yyyy)$|\(yy\)|\(yyyy\)/i;
const monthRegExp = /month/i;
const yearRegExp = /year/i;
const MMYYRegExp = /mm\s*(\/|\\)\s*yy/i;
const VisaCheckoutRegExp = /visa(-|\s)checkout/i;
const CREDIT_CARD_NETWORK_REGEXP = new RegExp(
  SUPPORTED_NETWORKS.concat(Object.keys(NETWORK_NAMES)).join("|"),
  "gui"
);
const TwoDigitYearRegExp = /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yy(?:[^y]|$)/i;
const FourDigitYearRegExp = /(?:exp.*date[^y\\n\\r]*|mm\\s*[-/]?\\s*)yyyy(?:[^y]|$)/i;
const dwfrmRegExp = /^dwfrm/i;
const bmlRegExp = /bml/i;
const templatedValue = /^\{\{.*\}\}$/;
const firstRegExp = /first/i;
const lastRegExp = /last/i;
const giftRegExp = /gift/i;
const subscriptionRegExp = /subscription/i;

function autocompleteStringMatches(element, ccString) {
  const info = getAutocompleteInfo(element);
  return info.fieldName === ccString;
}

function getFillableFormElements(element) {
  const formLike = FormLikeFactory.createFromField(element);
  return Array.from(formLike.elements).filter(el =>
    FormAutofillUtils.isFieldEligibleForAutofill(el)
  );
}

function nextFillableFormField(element) {
  const fillableFormElements = getFillableFormElements(element);
  const elementIndex = fillableFormElements.indexOf(element);
  return fillableFormElements[elementIndex + 1];
}

function previousFillableFormField(element) {
  const fillableFormElements = getFillableFormElements(element);
  const elementIndex = fillableFormElements.indexOf(element);
  return fillableFormElements[elementIndex - 1];
}

function nextFieldPredicateIsTrue(element, predicate) {
  const nextField = nextFillableFormField(element);
  return !!nextField && predicate(nextField);
}

function previousFieldPredicateIsTrue(element, predicate) {
  const previousField = previousFillableFormField(element);
  return !!previousField && predicate(previousField);
}

function nextFieldMatchesExpYearAutocomplete(fnode) {
  return nextFieldPredicateIsTrue(fnode.element, nextField =>
    autocompleteStringMatches(nextField, "cc-exp-year")
  );
}

function previousFieldMatchesExpMonthAutocomplete(fnode) {
  return previousFieldPredicateIsTrue(fnode.element, previousField =>
    autocompleteStringMatches(previousField, "cc-exp-month")
  );
}

//////////////////////////////////////////////
// Attribute Regular Expression Rules
function idOrNameMatchRegExp(element, regExp) {
  for (const str of [element.id, element.name]) {
    if (regExp.test(str)) {
      return true;
    }
  }
  return false;
}

function getElementLabels(element) {
  return {
    *[Symbol.iterator]() {
      const labels = LabelUtils.findLabelElements(element);
      for (let label of labels) {
        yield* LabelUtils.extractLabelStrings(label);
      }
    },
  };
}

function labelsMatchRegExp(element, regExp) {
  const elemStrings = getElementLabels(element);
  for (const str of elemStrings) {
    if (regExp.test(str)) {
      return true;
    }
  }
  return false;
}

function ariaLabelMatchesRegExp(element, regExp) {
  const ariaLabel = element.getAttribute("aria-label");
  return !!ariaLabel && regExp.test(ariaLabel);
}

function placeholderMatchesRegExp(element, regExp) {
  const placeholder = element.getAttribute("placeholder");
  return !!placeholder && regExp.test(placeholder);
}

function nextFieldIdOrNameMatchRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    idOrNameMatchRegExp(nextField, regExp)
  );
}

function nextFieldLabelsMatchRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    labelsMatchRegExp(nextField, regExp)
  );
}

function nextFieldPlaceholderMatchesRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    placeholderMatchesRegExp(nextField, regExp)
  );
}

function nextFieldAriaLabelMatchesRegExp(element, regExp) {
  return nextFieldPredicateIsTrue(element, nextField =>
    ariaLabelMatchesRegExp(nextField, regExp)
  );
}

function previousFieldIdOrNameMatchRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    idOrNameMatchRegExp(previousField, regExp)
  );
}

function previousFieldLabelsMatchRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    labelsMatchRegExp(previousField, regExp)
  );
}

function previousFieldPlaceholderMatchesRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    placeholderMatchesRegExp(previousField, regExp)
  );
}

function previousFieldAriaLabelMatchesRegExp(element, regExp) {
  return previousFieldPredicateIsTrue(element, previousField =>
    ariaLabelMatchesRegExp(previousField, regExp)
  );
}
//////////////////////////////////////////////

function isSelectWithCreditCardOptions(fnode) {
  // Check every select for options that match credit card network names in
  // value or label.
  const element = fnode.element;
  if (element.tagName === "SELECT") {
    for (let option of element.querySelectorAll("option")) {
      if (
        CreditCard.getNetworkFromName(option.value) ||
        CreditCard.getNetworkFromName(option.text)
      ) {
        return true;
      }
    }
  }
  return false;
}

/**
 * If any of the regular expressions match multiple times, we assume the tested
 * string belongs to a radio button for payment type instead of card type.
 *
 * @param {Fnode} fnode
 * @returns {boolean}
 */
function isRadioWithCreditCardText(fnode) {
  const element = fnode.element;
  const inputType = element.type;
  if (!!inputType && inputType === "radio") {
    const valueMatches = element.value.match(CREDIT_CARD_NETWORK_REGEXP);
    if (valueMatches) {
      return valueMatches.length === 1;
    }

    // Here we are checking that only one label matches only one entry in the regular expression.
    const labels = getElementLabels(element);
    let labelsMatched = 0;
    for (const label of labels) {
      const labelMatches = label.match(CREDIT_CARD_NETWORK_REGEXP);
      if (labelMatches) {
        if (labelMatches.length > 1) {
          return false;
        }
        labelsMatched++;
      }
    }
    if (labelsMatched > 0) {
      return labelsMatched === 1;
    }

    const textContentMatches = element.textContent.match(
      CREDIT_CARD_NETWORK_REGEXP
    );
    if (textContentMatches) {
      return textContentMatches.length === 1;
    }
  }
  return false;
}

function matchContiguousSubArray(array, subArray) {
  return array.some((elm, i) =>
    subArray.every((sElem, j) => sElem === array[i + j])
  );
}

function isExpirationMonthLikely(element) {
  if (element.tagName !== "SELECT") {
    return false;
  }

  const options = [...element.options];
  const desiredValues = Array(12)
    .fill(1)
    .map((v, i) => v + i);

  // The number of month options shouldn't be less than 12 or larger than 13
  // including the default option.
  if (options.length < 12 || options.length > 13) {
    return false;
  }

  return (
    matchContiguousSubArray(
      options.map(e => +e.value),
      desiredValues
    ) ||
    matchContiguousSubArray(
      options.map(e => +e.label),
      desiredValues
    )
  );
}

function isExpirationYearLikely(element) {
  if (element.tagName !== "SELECT") {
    return false;
  }

  const options = [...element.options];
  // A normal expiration year select should contain at least the last three years
  // in the list.
  const curYear = new Date().getFullYear();
  const desiredValues = Array(3)
    .fill(0)
    .map((v, i) => v + curYear + i);

  return (
    matchContiguousSubArray(
      options.map(e => +e.value),
      desiredValues
    ) ||
    matchContiguousSubArray(
      options.map(e => +e.label),
      desiredValues
    )
  );
}

function nextFieldIsExpirationYearLikely(fnode) {
  return nextFieldPredicateIsTrue(fnode.element, isExpirationYearLikely);
}

function previousFieldIsExpirationMonthLikely(fnode) {
  return previousFieldPredicateIsTrue(fnode.element, isExpirationMonthLikely);
}

function attrsMatchExpWith2Or4DigitYear(fnode, regExpMatchingFunction) {
  const element = fnode.element;
  return (
    regExpMatchingFunction(element, TwoDigitYearRegExp) ||
    regExpMatchingFunction(element, FourDigitYearRegExp)
  );
}

function maxLengthIs(fnode, maxLengthValue) {
  return fnode.element.maxLength === maxLengthValue;
}

function roleIsMenu(fnode) {
  const role = fnode.element.getAttribute("role");
  return !!role && role === "menu";
}

function idOrNameMatchDwfrmAndBml(fnode) {
  return (
    idOrNameMatchRegExp(fnode.element, dwfrmRegExp) &&
    idOrNameMatchRegExp(fnode.element, bmlRegExp)
  );
}

function hasTemplatedValue(fnode) {
  const value = fnode.element.getAttribute("value");
  return !!value && templatedValue.test(value);
}

function inputTypeNotNumbery(fnode) {
  const inputType = fnode.element.type;
  if (inputType) {
    return !["text", "tel", "number"].includes(inputType);
  }
  return false;
}

function isNotVisible(fnode) {
  try {
    return !isVisible(fnode);
  } catch (error) {
    if (error instanceof NoWindowError) {
      // This case should happen only during xpcshell tests that don't even
      // care about the info emitted from Fathom. Nevertheless, this is the
      // most likely truthful return value.
      return false;
    }
    throw error;
  }
}

function idOrNameMatchFirstAndLast(fnode) {
  return (
    idOrNameMatchRegExp(fnode.element, firstRegExp) &&
    idOrNameMatchRegExp(fnode.element, lastRegExp)
  );
}

/**
 * Compactly generate a series of rules that all take a single LHS type with no
 * .when() clause and have only a score() call on the right- hand side.
 *
 * @param {Lhs} inType The incoming fnode type that all rules take
 * @param {object} ruleMap A simple object used as a map with rule names
 *   pointing to scoring callbacks
 * @yields {Rule}
 */
function* simpleScoringRules(inType, ruleMap) {
  for (const [name, scoringCallback] of Object.entries(ruleMap)) {
    yield rule(type(inType), score(scoringCallback), { name });
  }
}

function makeRuleset(coeffs, biases) {
  return ruleset(
    [
      /**
       * Factor out the page scan just for a little more speed during training.
       * This selector is good for most fields. cardType is an exception: it
       * cannot be type=month.
       */
      rule(
        queriedOrClickedElements(
          "input:not([type]), input[type=text], input[type=textbox], input[type=email], input[type=tel], input[type=number], input[type=month], select, button"
        ),
        type("typicalCandidates")
      ),

      /**
       * number rules
       */
      rule(type("typicalCandidates"), type("cc-number")),
      ...simpleScoringRules("cc-number", {
        idOrNameMatchNumberRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        labelsMatchNumberRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-number"]),
        placeholderMatchesNumberRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        ariaLabelMatchesNumberRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-number"]
          ),
        idOrNameMatchGift: fnode =>
          idOrNameMatchRegExp(fnode.element, giftRegExp),
        labelsMatchGift: fnode => labelsMatchRegExp(fnode.element, giftRegExp),
        placeholderMatchesGift: fnode =>
          placeholderMatchesRegExp(fnode.element, giftRegExp),
        ariaLabelMatchesGift: fnode =>
          ariaLabelMatchesRegExp(fnode.element, giftRegExp),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isNotVisible,
        inputTypeNotNumbery,
      }),
      rule(type("cc-number"), out("cc-number")),

      /**
       * name rules
       */
      rule(type("typicalCandidates"), type("cc-name")),
      ...simpleScoringRules("cc-name", {
        idOrNameMatchNameRegExp: fnode =>
          idOrNameMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-name"]),
        labelsMatchNameRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-name"]),
        placeholderMatchesNameRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-name"]
          ),
        ariaLabelMatchesNameRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-name"]
          ),
        idOrNameMatchFirst: fnode =>
          idOrNameMatchRegExp(fnode.element, firstRegExp),
        labelsMatchFirst: fnode =>
          labelsMatchRegExp(fnode.element, firstRegExp),
        placeholderMatchesFirst: fnode =>
          placeholderMatchesRegExp(fnode.element, firstRegExp),
        ariaLabelMatchesFirst: fnode =>
          ariaLabelMatchesRegExp(fnode.element, firstRegExp),
        idOrNameMatchLast: fnode =>
          idOrNameMatchRegExp(fnode.element, lastRegExp),
        labelsMatchLast: fnode => labelsMatchRegExp(fnode.element, lastRegExp),
        placeholderMatchesLast: fnode =>
          placeholderMatchesRegExp(fnode.element, lastRegExp),
        ariaLabelMatchesLast: fnode =>
          ariaLabelMatchesRegExp(fnode.element, lastRegExp),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchFirstAndLast,
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isNotVisible,
      }),
      rule(type("cc-name"), out("cc-name")),

      /**
       * cardType rules
       */
      rule(
        queriedOrClickedElements(
          "input:not([type]), input[type=text], input[type=textbox], input[type=email], input[type=tel], input[type=number], input[type=radio], select, button"
        ),
        type("cc-type")
      ),
      ...simpleScoringRules("cc-type", {
        idOrNameMatchTypeRegExp: fnode =>
          idOrNameMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-type"]),
        labelsMatchTypeRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-type"]),
        idOrNameMatchVisaCheckout: fnode =>
          idOrNameMatchRegExp(fnode.element, VisaCheckoutRegExp),
        ariaLabelMatchesVisaCheckout: fnode =>
          ariaLabelMatchesRegExp(fnode.element, VisaCheckoutRegExp),
        isSelectWithCreditCardOptions,
        isRadioWithCreditCardText,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-type"), out("cc-type")),

      /**
       * expiration rules
       */
      rule(type("typicalCandidates"), type("cc-exp")),
      ...simpleScoringRules("cc-exp", {
        labelsMatchExpRegExp: fnode =>
          labelsMatchRegExp(fnode.element, HeuristicsRegExp.RULES["cc-exp"]),
        placeholderMatchesExpRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp"]
          ),
        labelsMatchExpWith2Or4DigitYear: fnode =>
          attrsMatchExpWith2Or4DigitYear(fnode, labelsMatchRegExp),
        placeholderMatchesExpWith2Or4DigitYear: fnode =>
          attrsMatchExpWith2Or4DigitYear(fnode, placeholderMatchesRegExp),
        labelsMatchMMYY: fnode => labelsMatchRegExp(fnode.element, MMYYRegExp),
        placeholderMatchesMMYY: fnode =>
          placeholderMatchesRegExp(fnode.element, MMYYRegExp),
        maxLengthIs7: fnode => maxLengthIs(fnode, 7),
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
        isExpirationMonthLikely: fnode =>
          isExpirationMonthLikely(fnode.element),
        isExpirationYearLikely: fnode => isExpirationYearLikely(fnode.element),
        idOrNameMatchMonth: fnode =>
          idOrNameMatchRegExp(fnode.element, monthRegExp),
        idOrNameMatchYear: fnode =>
          idOrNameMatchRegExp(fnode.element, yearRegExp),
        idOrNameMatchExpMonthRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        idOrNameMatchExpYearRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        idOrNameMatchValidation: fnode =>
          idOrNameMatchRegExp(fnode.element, /validate|validation/i),
      }),
      rule(type("cc-exp"), out("cc-exp")),

      /**
       * expirationMonth rules
       */
      rule(type("typicalCandidates"), type("cc-exp-month")),
      ...simpleScoringRules("cc-exp-month", {
        idOrNameMatchExpMonthRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        labelsMatchExpMonthRegExp: fnode =>
          labelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        placeholderMatchesExpMonthRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        ariaLabelMatchesExpMonthRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        idOrNameMatchMonth: fnode =>
          idOrNameMatchRegExp(fnode.element, monthRegExp),
        labelsMatchMonth: fnode =>
          labelsMatchRegExp(fnode.element, monthRegExp),
        placeholderMatchesMonth: fnode =>
          placeholderMatchesRegExp(fnode.element, monthRegExp),
        ariaLabelMatchesMonth: fnode =>
          ariaLabelMatchesRegExp(fnode.element, monthRegExp),
        nextFieldIdOrNameMatchExpYearRegExp: fnode =>
          nextFieldIdOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldLabelsMatchExpYearRegExp: fnode =>
          nextFieldLabelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldPlaceholderMatchExpYearRegExp: fnode =>
          nextFieldPlaceholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldAriaLabelMatchExpYearRegExp: fnode =>
          nextFieldAriaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        nextFieldIdOrNameMatchYear: fnode =>
          nextFieldIdOrNameMatchRegExp(fnode.element, yearRegExp),
        nextFieldLabelsMatchYear: fnode =>
          nextFieldLabelsMatchRegExp(fnode.element, yearRegExp),
        nextFieldPlaceholderMatchesYear: fnode =>
          nextFieldPlaceholderMatchesRegExp(fnode.element, yearRegExp),
        nextFieldAriaLabelMatchesYear: fnode =>
          nextFieldAriaLabelMatchesRegExp(fnode.element, yearRegExp),
        nextFieldMatchesExpYearAutocomplete,
        isExpirationMonthLikely: fnode =>
          isExpirationMonthLikely(fnode.element),
        nextFieldIsExpirationYearLikely,
        maxLengthIs2: fnode => maxLengthIs(fnode, 2),
        placeholderMatchesMM: fnode =>
          placeholderMatchesRegExp(fnode.element, MMRegExp),
        roleIsMenu,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-exp-month"), out("cc-exp-month")),

      /**
       * expirationYear rules
       */
      rule(type("typicalCandidates"), type("cc-exp-year")),
      ...simpleScoringRules("cc-exp-year", {
        idOrNameMatchExpYearRegExp: fnode =>
          idOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        labelsMatchExpYearRegExp: fnode =>
          labelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        placeholderMatchesExpYearRegExp: fnode =>
          placeholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        ariaLabelMatchesExpYearRegExp: fnode =>
          ariaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-year"]
          ),
        idOrNameMatchYear: fnode =>
          idOrNameMatchRegExp(fnode.element, yearRegExp),
        labelsMatchYear: fnode => labelsMatchRegExp(fnode.element, yearRegExp),
        placeholderMatchesYear: fnode =>
          placeholderMatchesRegExp(fnode.element, yearRegExp),
        ariaLabelMatchesYear: fnode =>
          ariaLabelMatchesRegExp(fnode.element, yearRegExp),
        previousFieldIdOrNameMatchExpMonthRegExp: fnode =>
          previousFieldIdOrNameMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldLabelsMatchExpMonthRegExp: fnode =>
          previousFieldLabelsMatchRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldPlaceholderMatchExpMonthRegExp: fnode =>
          previousFieldPlaceholderMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldAriaLabelMatchExpMonthRegExp: fnode =>
          previousFieldAriaLabelMatchesRegExp(
            fnode.element,
            HeuristicsRegExp.RULES["cc-exp-month"]
          ),
        previousFieldIdOrNameMatchMonth: fnode =>
          previousFieldIdOrNameMatchRegExp(fnode.element, monthRegExp),
        previousFieldLabelsMatchMonth: fnode =>
          previousFieldLabelsMatchRegExp(fnode.element, monthRegExp),
        previousFieldPlaceholderMatchesMonth: fnode =>
          previousFieldPlaceholderMatchesRegExp(fnode.element, monthRegExp),
        previousFieldAriaLabelMatchesMonth: fnode =>
          previousFieldAriaLabelMatchesRegExp(fnode.element, monthRegExp),
        previousFieldMatchesExpMonthAutocomplete,
        isExpirationYearLikely: fnode => isExpirationYearLikely(fnode.element),
        previousFieldIsExpirationMonthLikely,
        placeholderMatchesYYOrYYYY: fnode =>
          placeholderMatchesRegExp(fnode.element, YYorYYYYRegExp),
        roleIsMenu,
        idOrNameMatchSubscription: fnode =>
          idOrNameMatchRegExp(fnode.element, subscriptionRegExp),
        idOrNameMatchDwfrmAndBml,
        hasTemplatedValue,
      }),
      rule(type("cc-exp-year"), out("cc-exp-year")),
    ],
    coeffs,
    biases
  );
}

const coefficients = {
  "cc-number": [
    ["idOrNameMatchNumberRegExp", 6.3039140701293945],
    ["labelsMatchNumberRegExp", 2.8916432857513428],
    ["placeholderMatchesNumberRegExp", 5.505742073059082],
    ["ariaLabelMatchesNumberRegExp", 4.561432361602783],
    ["idOrNameMatchGift", -3.803224563598633],
    ["labelsMatchGift", -4.524861812591553],
    ["placeholderMatchesGift", -1.8712525367736816],
    ["ariaLabelMatchesGift", -3.674055576324463],
    ["idOrNameMatchSubscription", -0.7810876369476318],
    ["idOrNameMatchDwfrmAndBml", -1.095906138420105],
    ["hasTemplatedValue", -6.368256568908691],
    ["isNotVisible", -3.0330028533935547],
    ["inputTypeNotNumbery", -2.300889253616333],
  ],
  "cc-name": [
    ["idOrNameMatchNameRegExp", 7.953818321228027],
    ["labelsMatchNameRegExp", 11.784907341003418],
    ["placeholderMatchesNameRegExp", 9.202799797058105],
    ["ariaLabelMatchesNameRegExp", 9.627416610717773],
    ["idOrNameMatchFirst", -6.200107574462891],
    ["labelsMatchFirst", -14.77401065826416],
    ["placeholderMatchesFirst", -10.258772850036621],
    ["ariaLabelMatchesFirst", -2.1574606895446777],
    ["idOrNameMatchLast", -5.508854389190674],
    ["labelsMatchLast", -14.563374519348145],
    ["placeholderMatchesLast", -8.281961441040039],
    ["ariaLabelMatchesLast", -7.915995121002197],
    ["idOrNameMatchFirstAndLast", 17.586633682250977],
    ["idOrNameMatchSubscription", -1.3862149715423584],
    ["idOrNameMatchDwfrmAndBml", -1.154863953590393],
    ["hasTemplatedValue", -1.3476886749267578],
    ["isNotVisible", -20.619457244873047],
  ],
  "cc-type": [
    ["idOrNameMatchTypeRegExp", 2.815537691116333],
    ["labelsMatchTypeRegExp", -2.6969387531280518],
    ["idOrNameMatchVisaCheckout", -4.888851165771484],
    ["ariaLabelMatchesVisaCheckout", -5.0021514892578125],
    ["isSelectWithCreditCardOptions", 7.633410453796387],
    ["isRadioWithCreditCardText", 9.72647762298584],
    ["idOrNameMatchSubscription", -2.540968179702759],
    ["idOrNameMatchDwfrmAndBml", -2.4342823028564453],
    ["hasTemplatedValue", -2.134981155395508],
  ],
  "cc-exp": [
    ["labelsMatchExpRegExp", 7.235990524291992],
    ["placeholderMatchesExpRegExp", 3.7828152179718018],
    ["labelsMatchExpWith2Or4DigitYear", 3.28702449798584],
    ["placeholderMatchesExpWith2Or4DigitYear", 0.9417413473129272],
    ["labelsMatchMMYY", 8.527382850646973],
    ["placeholderMatchesMMYY", 6.976727485656738],
    ["maxLengthIs7", -1.6640985012054443],
    ["idOrNameMatchSubscription", -1.7390238046646118],
    ["idOrNameMatchDwfrmAndBml", -1.8697377443313599],
    ["hasTemplatedValue", -2.2890148162841797],
    ["isExpirationMonthLikely", -2.7287368774414062],
    ["isExpirationYearLikely", -2.1379034519195557],
    ["idOrNameMatchMonth", -2.9298980236053467],
    ["idOrNameMatchYear", -2.423668622970581],
    ["idOrNameMatchExpMonthRegExp", -2.224165916442871],
    ["idOrNameMatchExpYearRegExp", -2.4124796390533447],
    ["idOrNameMatchValidation", -6.64445686340332],
  ],
  "cc-exp-month": [
    ["idOrNameMatchExpMonthRegExp", 3.1759495735168457],
    ["labelsMatchExpMonthRegExp", 0.6333072781562805],
    ["placeholderMatchesExpMonthRegExp", -1.0211261510849],
    ["ariaLabelMatchesExpMonthRegExp", -0.12013287842273712],
    ["idOrNameMatchMonth", 0.8069844245910645],
    ["labelsMatchMonth", 2.8041117191314697],
    ["placeholderMatchesMonth", -0.7963107228279114],
    ["ariaLabelMatchesMonth", -0.18894313275814056],
    ["nextFieldIdOrNameMatchExpYearRegExp", 1.3703272342681885],
    ["nextFieldLabelsMatchExpYearRegExp", 0.4734393060207367],
    ["nextFieldPlaceholderMatchExpYearRegExp", -0.9648597240447998],
    ["nextFieldAriaLabelMatchExpYearRegExp", 2.3334436416625977],
    ["nextFieldIdOrNameMatchYear", 0.7225953936576843],
    ["nextFieldLabelsMatchYear", 0.47795572876930237],
    ["nextFieldPlaceholderMatchesYear", -1.032015085220337],
    ["nextFieldAriaLabelMatchesYear", 2.5017199516296387],
    ["nextFieldMatchesExpYearAutocomplete", 1.4952502250671387],
    ["isExpirationMonthLikely", 5.659104347229004],
    ["nextFieldIsExpirationYearLikely", 2.5078020095825195],
    ["maxLengthIs2", -0.5410940051078796],
    ["placeholderMatchesMM", 7.3071208000183105],
    ["roleIsMenu", 5.595693111419678],
    ["idOrNameMatchSubscription", -5.626739978790283],
    ["idOrNameMatchDwfrmAndBml", -7.236949920654297],
    ["hasTemplatedValue", -6.055515289306641],
  ],
  "cc-exp-year": [
    ["idOrNameMatchExpYearRegExp", 2.456799268722534],
    ["labelsMatchExpYearRegExp", 0.9488120675086975],
    ["placeholderMatchesExpYearRegExp", -0.6318328380584717],
    ["ariaLabelMatchesExpYearRegExp", -0.16433487832546234],
    ["idOrNameMatchYear", 2.0227997303009033],
    ["labelsMatchYear", 0.7777050733566284],
    ["placeholderMatchesYear", -0.6191908121109009],
    ["ariaLabelMatchesYear", -0.5337049961090088],
    ["previousFieldIdOrNameMatchExpMonthRegExp", 0.2529127597808838],
    ["previousFieldLabelsMatchExpMonthRegExp", 0.5853790044784546],
    ["previousFieldPlaceholderMatchExpMonthRegExp", -0.710956871509552],
    ["previousFieldAriaLabelMatchExpMonthRegExp", 2.2874839305877686],
    ["previousFieldIdOrNameMatchMonth", -1.99709153175354],
    ["previousFieldLabelsMatchMonth", 1.114603042602539],
    ["previousFieldPlaceholderMatchesMonth", -0.4987318515777588],
    ["previousFieldAriaLabelMatchesMonth", 2.1683783531188965],
    ["previousFieldMatchesExpMonthAutocomplete", 1.2016327381134033],
    ["isExpirationYearLikely", 5.7863616943359375],
    ["previousFieldIsExpirationMonthLikely", 6.4013848304748535],
    ["placeholderMatchesYYOrYYYY", 8.81661605834961],
    ["roleIsMenu", 3.7794034481048584],
    ["idOrNameMatchSubscription", -4.7467498779296875],
    ["idOrNameMatchDwfrmAndBml", -5.523425102233887],
    ["hasTemplatedValue", -6.14529275894165],
  ],
};

const biases = [
  ["cc-number", -4.422344207763672],
  ["cc-name", -5.876968860626221],
  ["cc-type", -5.410860061645508],
  ["cc-exp", -5.439330577850342],
  ["cc-exp-month", -5.99984073638916],
  ["cc-exp-year", -6.0192646980285645],
];

/**
 * END OF CODE PASTED FROM TRAINING REPOSITORY
 */

/**
 * MORE CODE UNIQUE TO PRODUCTION--NOT IN THE TRAINING REPOSITORY:
 */

this.creditCardRuleset = makeRuleset(
  [
    ...coefficients["cc-number"],
    ...coefficients["cc-name"],
    ...coefficients["cc-type"],
    ...coefficients["cc-exp"],
    ...coefficients["cc-exp-month"],
    ...coefficients["cc-exp-year"],
  ],
  biases
);
