/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderCalculator", "Calculator"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

// This pref is relative to the `browser.urlbar` branch.
const ENABLED_PREF = "suggest.calculator";

const DYNAMIC_RESULT_TYPE = "calculator";

const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "content",
      tag: "span",
      attributes: { class: "urlbarView-no-wrap" },
      children: [
        {
          name: "icon",
          tag: "img",
          attributes: { class: "urlbarView-favicon" },
        },
        {
          name: "input",
          tag: "strong",
        },
        {
          name: "action",
          tag: "span",
        },
      ],
    },
  ],
};

// Minimum number of parts of the expression before we show a result.
const MIN_EXPRESSION_LENGTH = 3;

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderCalculator extends UrlbarProvider {
  constructor() {
    super();
    UrlbarResult.addDynamicResultType(DYNAMIC_RESULT_TYPE);
    UrlbarView.addDynamicViewTemplate(DYNAMIC_RESULT_TYPE, VIEW_TEMPLATE);
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return DYNAMIC_RESULT_TYPE;
  }

  /**
   * The type of the provider.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return (
      queryContext.trimmedSearchString &&
      !queryContext.searchMode &&
      UrlbarPrefs.get(ENABLED_PREF)
    );
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    try {
      // Calculator will throw when given an invalid expression, therefore
      // addCallback will never be called.
      let postfix = Calculator.infix2postfix(queryContext.searchString);
      if (postfix.length < MIN_EXPRESSION_LENGTH) {
        return;
      }
      let value = Calculator.evaluatePostfix(postfix);
      const result = new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.DYNAMIC,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          value,
          input: queryContext.searchString,
          dynamicType: DYNAMIC_RESULT_TYPE,
        }
      );
      result.suggestedIndex = 1;
      addCallback(this, result);
    } catch (e) {}
  }

  getViewUpdate(result) {
    const viewUpdate = {
      icon: {
        attributes: {
          src: "chrome://global/skin/icons/edit-copy.svg",
        },
      },
      input: {
        l10n: {
          id: "urlbar-result-action-calculator-result",
          args: { result: result.payload.value },
        },
      },
      action: {
        l10n: { id: "urlbar-result-action-copy-to-clipboard" },
      },
    };

    return viewUpdate;
  }

  async pickResult({ payload }) {
    ClipboardHelper.copyString(payload.value);
  }
}

/**
 * Base implementation of a basic calculator.
 */
class BaseCalculator {
  // Holds the current symbols for calculation
  stack = [];
  numberSystems = [];

  addNumberSystem(system) {
    this.numberSystems.push(system);
  }

  isNumeric(value) {
    return value - 0 == value && value.length;
  }

  isOperator(value) {
    return this.numberSystems.some(sys => sys.isOperator(value));
  }

  isNumericToken(char) {
    return this.numberSystems.some(sys => sys.isNumericToken(char));
  }

  parsel10nFloat(num) {
    for (const system of this.numberSystems) {
      num = system.transformNumber(num);
    }
    return parseFloat(num, 10);
  }

  precedence(val) {
    if (["-", "+"].includes(val)) {
      return 2;
    }
    if (["*", "/"].includes(val)) {
      return 3;
    }

    return null;
  }

  // This is a basic implementation of the shunting yard algorithm
  // described http://en.wikipedia.org/wiki/Shunting-yard_algorithm
  // Currently functions are unimplemented and only operators with
  // left association are used
  infix2postfix(infix) {
    let parser = new Parser(infix, this);
    let tokens = parser.parse(infix);
    let output = [];
    let stack = [];

    tokens.forEach(token => {
      if (token.number) {
        output.push(this.parsel10nFloat(token.value, 10));
      }

      if (this.isOperator(token.value)) {
        let i = this.precedence;
        while (
          stack.length &&
          this.isOperator(stack[stack.length - 1]) &&
          i(token.value) <= i(stack[stack.length - 1])
        ) {
          output.push(stack.pop());
        }
        stack.push(token.value);
      }

      if (token.value === "(") {
        stack.push(token.value);
      }

      if (token.value === ")") {
        while (stack.length && stack[stack.length - 1] !== "(") {
          output.push(stack.pop());
        }
        // This is the (
        stack.pop();
      }
    });

    while (stack.length) {
      output.push(stack.pop());
    }
    return output;
  }

  evaluate = {
    "*": (a, b) => a * b,
    "+": (a, b) => a + b,
    "-": (a, b) => a - b,
    "/": (a, b) => a / b,
  };

  evaluatePostfix(postfix) {
    let stack = [];

    postfix.forEach(token => {
      if (!this.isOperator(token)) {
        stack.push(token);
      } else {
        let op2 = stack.pop();
        let op1 = stack.pop();
        let result = this.evaluate[token](op1, op2);
        if (isNaN(result)) {
          throw new Error("Value is " + result);
        }
        stack.push(result);
      }
    });
    let finalResult = stack.pop();
    if (isNaN(finalResult)) {
      throw new Error("Value is " + finalResult);
    }
    return finalResult;
  }
}

function Parser(input, calculator) {
  this.calculator = calculator;
  this.init(input);
}

Parser.prototype = {
  init(input) {
    // No spaces.
    input = input.replace(/[ \t\v\n]/g, "");

    // String to array:
    this._chars = [];
    for (let i = 0; i < input.length; ++i) {
      this._chars.push(input[i]);
    }

    this._tokens = [];
  },

  // This method returns an array of objects with these properties:
  // - number: true/false
  // - value:  the token value
  parse(input) {
    // The input must be a "block" without any digit left.
    if (!this._tokenizeBlock() || this._chars.length) {
      throw new Error("Wrong input");
    }

    return this._tokens;
  },

  _tokenizeBlock() {
    if (!this._chars.length) {
      return false;
    }

    // "(" + something + ")"
    if (this._chars[0] == "(") {
      this._tokens.push({ number: false, value: this._chars[0] });
      this._chars.shift();

      if (!this._tokenizeBlock()) {
        return false;
      }

      if (!this._chars.length || this._chars[0] != ")") {
        return false;
      }

      this._chars.shift();

      this._tokens.push({ number: false, value: ")" });
    } else if (!this._tokenizeNumber()) {
      // number + ...
      return false;
    }

    if (!this._chars.length || this._chars[0] == ")") {
      return true;
    }

    while (this._chars.length && this._chars[0] != ")") {
      if (!this._tokenizeOther()) {
        return false;
      }

      if (!this._tokenizeBlock()) {
        return false;
      }
    }

    return true;
  },

  // This is a simple float parser.
  _tokenizeNumber() {
    if (!this._chars.length) {
      return false;
    }

    // {+,-}something
    let number = [];
    if (/[+-]/.test(this._chars[0])) {
      number.push(this._chars.shift());
    }

    let tokenizeNumberInternal = () => {
      if (
        !this._chars.length ||
        !this.calculator.isNumericToken(this._chars[0])
      ) {
        return false;
      }

      while (
        this._chars.length &&
        this.calculator.isNumericToken(this._chars[0])
      ) {
        number.push(this._chars.shift());
      }

      return true;
    };

    if (!tokenizeNumberInternal()) {
      return false;
    }

    // 123{e...}
    if (!this._chars.length || this._chars[0] != "e") {
      this._tokens.push({ number: true, value: number.join("") });
      return true;
    }

    number.push(this._chars.shift());

    // 123e{+,-}
    if (/[+-]/.test(this._chars[0])) {
      number.push(this._chars.shift());
    }

    if (!this._chars.length) {
      return false;
    }

    // the number
    if (!tokenizeNumberInternal()) {
      return false;
    }

    this._tokens.push({ number: true, value: number.join("") });
    return true;
  },

  _tokenizeOther() {
    if (!this._chars.length) {
      return false;
    }

    if (this.calculator.isOperator(this._chars[0])) {
      this._tokens.push({ number: false, value: this._chars.shift() });
      return true;
    }

    return false;
  },
};

let Calculator = new BaseCalculator();

Calculator.addNumberSystem({
  isOperator: char => ["÷", "×", "-", "+", "*", "/"].includes(char),
  isNumericToken: char => /^[0-9\.,]/.test(char),
  // parseFloat will only handle numbers that use periods as decimal
  // seperators, various countries use commas. This function attempts
  // to fixup the number so parseFloat will accept it.
  transformNumber: num => {
    let firstComma = num.indexOf(",");
    let firstPeriod = num.indexOf(".");

    if (firstPeriod != -1 && firstComma != -1 && firstPeriod < firstComma) {
      // Contains both a period and a comma and the period came first
      // so using comma as decimal seperator, strip . and replace , with .
      // (ie 1.999,5).
      num = num.replace(/\./g, "");
      num = num.replace(/,/g, ".");
    } else if (firstPeriod != -1 && firstComma != -1) {
      // Contains both a period and a comma and the comma came first
      // so strip the comma (ie 1,999.5).
      num = num.replace(/,/g, "");
    } else if (firstComma != -1) {
      // Has commas but no periods so treat comma as decimal seperator
      num = num.replace(/,/g, ".");
    }
    return num;
  },
});

var UrlbarProviderCalculator = new ProviderCalculator();
